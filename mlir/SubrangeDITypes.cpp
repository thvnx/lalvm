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

  // Collect constrained integer subtypes (any recorded range, static or
  // dynamic). A static bound becomes a constant; a dynamic one references the
  // bound variable AdaDebugInfoPass recorded for it (a parameter's own
  // variable, or an artificial one for a computed or non-surviving-local
  // bound).
  llvm::SmallVector<mlir::ada::TypeOp> subtypeOps;
  module.walk([&](mlir::ada::TypeOp typeOp) {
    auto intInfo = mlir::dyn_cast_or_null<mlir::ada::IntegerTypeInfoAttr>(
        typeOp.getTypeInfoAttr());
    if (intInfo && typeOp.getBaseAttr() && intInfo.hasRange() &&
        mlir::isa<mlir::IntegerType>(typeOp.getMlirType()))
      subtypeOps.push_back(typeOp);
  });
  if (subtypeOps.empty())
    return;

  // Index the artificial bound variables (short name + bound suffix) emitted by
  // AdaDebugInfoPass, keyed by (scope, name): the name is unique only within
  // the subprogram where the subtype is declared, so a dynamic bound looks it
  // up in that scope.
  llvm::DenseMap<std::pair<llvm::DILocalScope *, llvm::StringRef>,
                 llvm::DILocalVariable *>
      boundVarByScopeName;
  for (llvm::Function &f : llvmModule)
    for (llvm::BasicBlock &bb : f)
      for (llvm::Instruction &i : bb)
        for (llvm::DbgVariableRecord &dvr :
             llvm::filterDbgVars(i.getDbgRecordRange())) {
          auto *v = dvr.getVariable();
          boundVarByScopeName[{v->getScope(), v->getName()}] = v;
        }

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

    // Scope from the enclosing-subprogram metadata: a locally-declared subtype
    // is scoped to its subprogram. findInstanceOf walks any FusedLoc layering
    // (a dynamic subtype also carries DISubrangeBounds metadata).
    llvm::DIScope *scope = cu;
    if (auto fused = typeOp.getLoc()
                         ->findInstanceOf<
                             mlir::FusedLocWith<mlir::ada::DIScopeRefAttr>>()) {
      auto *fn =
          llvmModule.getFunction(fused.getMetadata().getScope().getValue());
      if (fn && fn->getSubprogram())
        scope = fn->getSubprogram();
    }
    // Unwrap the FusedLoc metadata layers down to the source location.
    mlir::Location loc = typeOp.getLoc();
    while (auto fused = mlir::dyn_cast<mlir::FusedLoc>(loc)) {
      if (fused.getLocations().empty())
        break;
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
    // A static bound is a constant; a dynamic one references the variable
    // AdaDebugInfoPass recorded for it (the source object, or an artificial
    // bound) as DISubrangeBounds metadata, found by name within this
    // subprogram's scope.
    auto *subprogram = llvm::dyn_cast_or_null<llvm::DISubprogram>(scope);
    auto bounds =
        typeOp.getLoc()
            ->findInstanceOf<
                mlir::FusedLocWith<mlir::ada::DISubrangeBoundsAttr>>();
    auto dynBound = [&](mlir::StringAttr var) -> llvm::Metadata * {
      if (!subprogram || !var)
        return nullptr;
      auto it = boundVarByScopeName.find({subprogram, var.getValue()});
      return it == boundVarByScopeName.end() ? nullptr : it->second;
    };
    llvm::Metadata *loMD =
        intInfo.staticLower()
            ? boundMD(intInfo.staticLower(), boundTy)
            : dynBound(bounds ? bounds.getMetadata().getLower() : nullptr);
    llvm::Metadata *hiMD =
        intInfo.staticUpper()
            ? boundMD(intInfo.staticUpper(), boundTy)
            : dynBound(bounds ? bounds.getMetadata().getUpper() : nullptr);
    auto *subrange = db.createSubrangeType(
        mlir::ada::bareName(typeOp.getSymName()), getOrCreateFile(filePath),
        line, scope, intType.getWidth(), /*AlignInBits=*/0,
        llvm::DINode::FlagZero, baseTypeFor(typeOp), loMD, hiMD,
        /*Stride=*/nullptr, /*Bias=*/nullptr);
    subrangeByName[typeOp.getSymName()] = subrange;
  }

  // Replace the typedef placeholders (emitted by AdaDebugInfoPass) with the
  // full DISubrangeType built above, matching by the subtype's sym_name.
  for (auto &f : llvmModule) {
    for (auto &bb : f) {
      for (auto &i : bb) {
        for (llvm::DbgVariableRecord &dvr :
             llvm::filterDbgVars(i.getDbgRecordRange())) {
          auto *var = dvr.getVariable();
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
          dvr.setVariable(newVar);
        }
      }
    }
  }

  // Rewrite subprogram signatures: a return or parameter type still pointing at
  // the typedef placeholder becomes the subrange, so the DISubroutineType
  // matches the parameter DIEs and leaves no placeholder behind.
  for (llvm::Function &f : llvmModule) {
    auto *sp = f.getSubprogram();
    if (!sp || !sp->getType() || !sp->getType()->getRawTypeArray())
      continue;
    llvm::SmallVector<llvm::Metadata *> elems;
    bool changed = false;
    for (llvm::DIType *t : sp->getType()->getTypeArray()) {
      if (auto *deriv = llvm::dyn_cast_or_null<llvm::DIDerivedType>(t)) {
        auto it = subrangeByName.find(deriv->getName());
        if (it != subrangeByName.end()) {
          elems.push_back(it->second);
          changed = true;
          continue;
        }
      }
      elems.push_back(t);
    }
    if (changed)
      sp->replaceType(db.createSubroutineType(db.getOrCreateTypeArray(elems)));
  }

  db.finalize();
}
