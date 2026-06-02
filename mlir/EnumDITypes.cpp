//===- EnumDITypes.cpp - Post-translation enum DI type builder ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ada/EnumDITypes.h"
#include "ada/Dialect.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/BuiltinTypes.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugProgramInstruction.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/Path.h"

void mlir::ada::buildEnumDITypes(llvm::Module &llvmModule,
                                 mlir::ModuleOp module) {
  auto *cuMeta = llvmModule.getNamedMetadata("llvm.dbg.cu");
  if (!cuMeta || cuMeta->getNumOperands() == 0)
    return;
  auto *cu = llvm::cast<llvm::DICompileUnit>(cuMeta->getOperand(0));

  // Collect surviving enum ada.type ops; skip the IR scan if none exist.
  llvm::SmallVector<mlir::ada::TypeOp> enumTypeOps;
  module.walk([&](mlir::ada::TypeOp typeOp) {
    if (mlir::isa<mlir::ada::EnumTypeInfoAttr>(typeOp.getTypeInfo()) &&
        mlir::isa<mlir::IntegerType>(typeOp.getMlirType()))
      enumTypeOps.push_back(typeOp);
  });
  if (enumTypeOps.empty())
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

  // Build enum DI types directly from surviving ada.type ops.
  llvm::StringMap<llvm::DICompositeType *> enumTypeByName;
  for (mlir::ada::TypeOp typeOp : enumTypeOps) {
    auto enumInfo =
        mlir::cast<mlir::ada::EnumTypeInfoAttr>(typeOp.getTypeInfo());
    auto intType = mlir::cast<mlir::IntegerType>(typeOp.getMlirType());

    llvm::StringRef filePath;
    unsigned line = 0;
    if (auto flc = mlir::dyn_cast<mlir::FileLineColRange>(typeOp.getLoc())) {
      filePath = flc.getFilename().getValue();
      line = flc.getStartLine();
    }

    llvm::DIScope *scope = cu;
    if (auto func = typeOp->getParentOfType<mlir::LLVM::LLVMFuncOp>()) {
      auto *fn = llvmModule.getFunction(func.getName());
      if (fn && fn->getSubprogram())
        scope = fn->getSubprogram();
    }

    llvm::SmallVector<llvm::Metadata *, 8> elems;
    for (auto [name, val] : enumInfo.literals())
      elems.push_back(db.createEnumerator(name, static_cast<uint64_t>(val)));

    // The displayed DWARF name is the bare Ada name; the lookup key below stays
    // the full sym_name so shadowed enums (same bare name) remain distinct.
    auto *enumType = db.createEnumerationType(
        scope, mlir::ada::bareName(typeOp.getSymName()),
        getOrCreateFile(filePath), line, llvm::alignTo(intType.getWidth(), 8),
        /*AlignInBits=*/0, db.getOrCreateArray(elems),
        /*UnderlyingType=*/nullptr);
    enumTypeByName[typeOp.getSymName()] = enumType;
  }

  // Replace empty DICompositeType stubs (emitted by AdaDebugInfoPass as
  // placeholders for enum types) with the full DICompositeType built above.
  for (auto &F : llvmModule) {
    for (auto &BB : F) {
      for (auto &I : BB) {
        for (llvm::DbgVariableRecord &DVR :
             llvm::filterDbgVars(I.getDbgRecordRange())) {
          auto *var = DVR.getVariable();
          auto *ct = llvm::dyn_cast<llvm::DICompositeType>(var->getType());
          if (!ct || !ct->getElements().empty())
            continue;
          auto it = enumTypeByName.find(ct->getName());
          if (it == enumTypeByName.end())
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
