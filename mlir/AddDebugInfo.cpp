//===- AddDebugInfo.cpp - Collect Ada type metadata for DWARF -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements AddAdaDebugInfoPass, which collects Ada enum type
// metadata before `ada.type` ops are erased by LowerToLLVM. Emission is split
// from lowering because MLIR 21 lacks DIEnumeratorAttr and retainedTypes on
// DICompileUnitAttr, requiring LLVM's native DIBuilder API after translation.
//
// Pipeline overview (see lalvm.cpp::dumpLLVMIR):
//
//   AddAdaDebugInfoPass(enumInfos)
//     -> populate enumInfos from ada.type ops with EnumTypeInfoAttr; no IR
//        mutations
//   LowerToLLVMPass
//     -> lower Ada ops; ada.type ops survive (marked legal)
//   FinalizeAdaObjectPass
//     -> emit LLVM debug intrinsics for variables and parameters
//
//   After translateModuleToLLVMIR, lalvm.cpp::attachAdaDebugInfo consumes
//   enumInfos to emit DW_TAG_enumeration_type via DIBuilder::retainType.
//
//===----------------------------------------------------------------------===//

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;

namespace {

struct AddAdaDebugInfoPass
    : public PassWrapper<AddAdaDebugInfoPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(AddAdaDebugInfoPass)

  explicit AddAdaDebugInfoPass(llvm::SmallVector<ada::AdaEnumInfo> &infos)
      : enumInfos(infos) {}

  void runOnOperation() override {
    getOperation().walk([&](ada::TypeOp typeOp) {
      auto enumInfo =
          mlir::dyn_cast<ada::EnumTypeInfoAttr>(typeOp.getTypeInfo());
      if (!enumInfo)
        return;
      auto intType = mlir::dyn_cast<mlir::IntegerType>(typeOp.getMlirType());
      if (!intType)
        return;
      llvm::SmallVector<std::string> names;
      for (llvm::StringRef name : enumInfo.getNames())
        names.push_back(name.str());
      llvm::SmallVector<int64_t> values(enumInfo.getValues().begin(),
                                        enumInfo.getValues().end());

      std::optional<std::string> subpScope;
      for (mlir::Operation *p = typeOp->getParentOp(); p; p = p->getParentOp())
        if (auto enclosing = mlir::dyn_cast<ada::SubpOp>(p)) {
          subpScope = enclosing.getMangledName();
          break;
        }

      enumInfos.push_back({typeOp.getSymName().str(), intType.getWidth(),
                           typeOp.getLoc(), std::move(names), std::move(values),
                           std::move(subpScope)});
    });
  }

  llvm::SmallVector<ada::AdaEnumInfo> &enumInfos;
};
} // namespace

std::unique_ptr<mlir::Pass> mlir::ada::createAddAdaDebugInfoPass(
    llvm::SmallVector<AdaEnumInfo> &enumInfos) {
  return std::make_unique<AddAdaDebugInfoPass>(enumInfos);
}
