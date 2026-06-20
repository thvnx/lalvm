//===- EnumDITypes.cpp - Post-translation enum DI type builder ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ada/EnumDITypes.h"
#include "ada/DITypeUtils.h"
#include "ada/Dialect.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugProgramInstruction.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/MathExtras.h"

void mlir::ada::buildEnumDITypes(llvm::Module &llvmModule,
                                 mlir::ModuleOp module) {
  auto *cuMeta = llvmModule.getNamedMetadata("llvm.dbg.cu");
  if (!cuMeta || cuMeta->getNumOperands() == 0)
    return;
  auto *cu = llvm::cast<llvm::DICompileUnit>(cuMeta->getOperand(0));

  // Collect surviving enum ada.type ops; skip the IR scan if none exist.
  llvm::SmallVector<mlir::ada::TypeOp> enumTypeOps;
  module.walk([&](mlir::ada::TypeOp typeOp) {
    // Constraint-only enum subtype infos carry no literals to describe.
    auto enumInfo = mlir::dyn_cast_or_null<mlir::ada::EnumTypeInfoAttr>(
        typeOp.getTypeInfoAttr());
    if (enumInfo && !enumInfo.getNames().empty() &&
        mlir::isa<mlir::IntegerType>(typeOp.getMlirType()))
      enumTypeOps.push_back(typeOp);
  });
  if (enumTypeOps.empty())
    return;

  llvm::DIBuilder db(llvmModule, /*AllowUnresolved=*/false, cu);
  llvm::DenseMap<llvm::StringRef, llvm::DIFile *> fileCache;

  // Build enum DI types directly from surviving ada.type ops.
  llvm::StringMap<llvm::DICompositeType *> enumTypeByName;
  for (mlir::ada::TypeOp typeOp : enumTypeOps) {
    auto enumInfo =
        mlir::cast<mlir::ada::EnumTypeInfoAttr>(typeOp.getTypeInfoAttr());
    auto intType = mlir::cast<mlir::IntegerType>(typeOp.getMlirType());

    // HoistNestedSymbolOperations fused the enclosing subprogram (DWARF scope)
    // onto the type's location before lifting it to module level, away from its
    // lexical parent. Unwrap it for both the scope and the source file/line; a
    // type without it (predefined / library-level) is scoped to the cu.
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

    llvm::SmallVector<llvm::Metadata *, 8> elems;
    for (auto [name, val] : enumInfo.literals())
      elems.push_back(db.createEnumerator(name, static_cast<uint64_t>(val)));

    // The displayed DWARF name is the bare Ada name; the lookup key below stays
    // the full sym_name so shadowed enums (same bare name) remain distinct.
    auto *enumType = db.createEnumerationType(
        scope, mlir::ada::bareName(typeOp.getSymName()),
        mlir::ada::getOrCreateDIFile(db, fileCache, filePath), line,
        llvm::alignTo(intType.getWidth(), 8),
        /*AlignInBits=*/0, db.getOrCreateArray(elems),
        /*UnderlyingType=*/nullptr);
    enumTypeByName[typeOp.getSymName()] = enumType;
  }

  // Replace empty DICompositeType stubs (emitted by AdaDebugInfoPass as
  // placeholders for enum types) with the full DICompositeType built above.
  for (auto &f : llvmModule) {
    for (auto &bb : f) {
      for (auto &i : bb) {
        for (llvm::DbgVariableRecord &dvr :
             llvm::filterDbgVars(i.getDbgRecordRange())) {
          auto *ct = llvm::dyn_cast<llvm::DICompositeType>(
              dvr.getVariable()->getType());
          if (!ct || !ct->getElements().empty())
            continue;
          auto it = enumTypeByName.find(ct->getName());
          if (it == enumTypeByName.end())
            continue;
          mlir::ada::retypeLocalVariable(db, dvr, it->second);
        }
      }
    }
  }

  db.finalize();
}
