//===- SubrangeDITypes.cpp - Post-translation subrange DI type builder ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ada/SubrangeDITypes.h"
#include "ada/Dialect.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/SymbolTable.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugProgramInstruction.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Path.h"

void mlir::ada::buildSubrangeDITypes(llvm::Module &llvmModule,
                                     mlir::ModuleOp module) {
  auto *cuMeta = llvmModule.getNamedMetadata("llvm.dbg.cu");
  if (!cuMeta || cuMeta->getNumOperands() == 0)
    return;
  auto *cu = llvm::cast<llvm::DICompileUnit>(cuMeta->getOperand(0));

  // Collect constrained integer subtypes carrying at least one static bound;
  // a subtype with only dynamic bounds keeps its basic type for now.
  llvm::SmallVector<mlir::ada::TypeOp> subtypeOps;
  module.walk([&](mlir::ada::TypeOp typeOp) {
    auto intInfo = mlir::dyn_cast_or_null<mlir::ada::IntegerTypeInfoAttr>(
        typeOp.getTypeInfoAttr());
    if (intInfo && typeOp.getBaseAttr() &&
        (intInfo.staticLower() || intInfo.staticUpper()) &&
        mlir::isa<mlir::IntegerType>(typeOp.getMlirType()))
      subtypeOps.push_back(typeOp);
  });
  if (subtypeOps.empty())
    return;

  llvm::DIBuilder db(llvmModule, /*AllowUnresolved=*/false, cu);
  llvm::DenseMap<llvm::StringRef, llvm::DIFile *> fileCache;
  auto getOrCreateFile = [&](llvm::StringRef filePath) -> llvm::DIFile * {
    auto *&file = fileCache[filePath];
    if (!file)
      file = db.createFile(llvm::sys::path::filename(filePath),
                           llvm::sys::path::parent_path(filePath));
    return file;
  };

  // A static bound becomes a constant in the subtype's own machine width (the
  // `int_info` encoding is a minimal-width signed value, widened to fit), so a
  // wider-than-64-bit subtype keeps its bounds exact.
  auto boundMD = [&](mlir::IntegerAttr bound,
                     llvm::IntegerType *ty) -> llvm::Metadata * {
    if (!bound)
      return nullptr;
    return llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(
        ty, bound.getValue().sextOrTrunc(ty->getBitWidth())));
  };

  // The base type DIE is shared across subtypes of the same base.
  llvm::StringMap<llvm::DIType *> baseTypeByName;
  auto baseTypeFor = [&](mlir::ada::TypeOp typeOp) -> llvm::DIType * {
    auto baseOp = mlir::dyn_cast_or_null<mlir::ada::TypeOp>(
        mlir::SymbolTable::lookupNearestSymbolFrom(typeOp,
                                                   typeOp.getBaseAttr()));
    if (!baseOp)
      return nullptr;
    auto *&cached = baseTypeByName[baseOp.getSymName()];
    if (!cached) {
      unsigned enc = llvm::dwarf::DW_ATE_signed;
      if (auto baseInfo =
              mlir::dyn_cast_or_null<mlir::ada::IntegerTypeInfoAttr>(
                  baseOp.getTypeInfoAttr()))
        if (baseInfo.getModulus())
          enc = llvm::dwarf::DW_ATE_unsigned;
      cached = db.createBasicType(
          mlir::ada::bareName(baseOp.getSymName()),
          mlir::cast<mlir::IntegerType>(baseOp.getMlirType()).getWidth(), enc);
    }
    return cached;
  };

  llvm::StringMap<llvm::DISubrangeType *> subrangeByName;
  for (mlir::ada::TypeOp typeOp : subtypeOps) {
    auto intInfo =
        mlir::cast<mlir::ada::IntegerTypeInfoAttr>(typeOp.getTypeInfoAttr());
    auto intType = mlir::cast<mlir::IntegerType>(typeOp.getMlirType());

    // Scope/file/line from the fused location (same unwrap as
    // buildEnumDITypes): a locally-declared subtype is scoped to its enclosing
    // subprogram.
    mlir::Location loc = typeOp.getLoc();
    llvm::DIScope *scope = cu;
    if (auto fused = mlir::dyn_cast<mlir::FusedLoc>(loc)) {
      if (auto scopeRef = mlir::dyn_cast_or_null<mlir::ada::DIScopeRefAttr>(
              fused.getMetadata())) {
        auto *fn = llvmModule.getFunction(scopeRef.getScope().getValue());
        if (fn && fn->getSubprogram())
          scope = fn->getSubprogram();
      }
      if (!fused.getLocations().empty())
        loc = fused.getLocations().front();
    }
    llvm::StringRef filePath;
    unsigned line = 0;
    if (auto flc = mlir::dyn_cast<mlir::FileLineColRange>(loc)) {
      filePath = flc.getFilename().getValue();
      line = flc.getStartLine();
    }

    auto *boundTy =
        llvm::IntegerType::get(llvmModule.getContext(), intType.getWidth());
    auto *subrange = db.createSubrangeType(
        mlir::ada::bareName(typeOp.getSymName()), getOrCreateFile(filePath),
        line, scope, intType.getWidth(), /*AlignInBits=*/0,
        llvm::DINode::FlagZero, baseTypeFor(typeOp),
        boundMD(intInfo.staticLower(), boundTy),
        boundMD(intInfo.staticUpper(), boundTy),
        /*Stride=*/nullptr, /*Bias=*/nullptr);
    subrangeByName[typeOp.getSymName()] = subrange;
  }

  // Replace the typedef placeholders (emitted by AdaDebugInfoPass) with the
  // full DISubrangeType built above, matching by the subtype's sym_name.
  for (auto &F : llvmModule) {
    for (auto &BB : F) {
      for (auto &I : BB) {
        for (llvm::DbgVariableRecord &DVR :
             llvm::filterDbgVars(I.getDbgRecordRange())) {
          auto *var = DVR.getVariable();
          auto *dt = llvm::dyn_cast<llvm::DIDerivedType>(var->getType());
          if (!dt)
            continue;
          auto it = subrangeByName.find(dt->getName());
          if (it == subrangeByName.end())
            continue;
          llvm::DILocalVariable *newVar;
          if (var->getArg() > 0)
            newVar = db.createParameterVariable(var->getScope(), var->getName(),
                                                var->getArg(), var->getFile(),
                                                var->getLine(), it->second);
          else
            newVar = db.createAutoVariable(var->getScope(), var->getName(),
                                           var->getFile(), var->getLine(),
                                           it->second);
          DVR.setVariable(newVar);
        }
      }
    }
  }

  db.finalize();
}
