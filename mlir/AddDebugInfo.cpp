//===- AddDebugInfo.cpp - Collect Ada type metadata for DWARF -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements AddAdaDebugInfoPass, the first stage of Ada DWARF debug
// info emission. Emission is split into two stages because MLIR 21 lacks
// DIEnumeratorAttr and retainedTypes on DICompileUnitAttr, so the final DWARF
// construction must go through LLVM's native DIBuilder API after MLIR-to-LLVM
// IR translation.
//
// Pipeline overview (see lalvm.cpp::dumpLLVMIR for the full sequence):
//
//   lalvm.cpp::dumpLLVMIR owns SmallVector<AdaEnumInfo> enumInfos and
//   SmallVector<AdaParamInfo> paramInfos and passes them by reference into
//   applyLoweringPasses, which runs:
//
//     AddAdaDebugInfoPass(enumInfos, paramInfos)
//       -> populate enumInfos from ada.type ops (with subpScope for locally
//          declared types) and paramInfos from "ada.type" arg_attrs on
//          ada.subp ops; no IR mutations
//     LowerToLLVMPass
//       -> erase ada.type ops and lower remaining Ada ops
//
//   After translateModuleToLLVMIR, lalvm.cpp::attachAdaDebugInfo consumes
//   enumInfos and paramInfos to emit DW_TAG_enumeration_type via
//   DIBuilder::retainType and DW_TAG_formal_parameter via
//   DIBuilder::createParameterVariable.
//
//===----------------------------------------------------------------------===//

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;

namespace {

struct AddAdaDebugInfoPass
    : public PassWrapper<AddAdaDebugInfoPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(AddAdaDebugInfoPass)

  explicit AddAdaDebugInfoPass(llvm::SmallVector<ada::AdaEnumInfo> &infos,
                               llvm::SmallVector<ada::AdaParamInfo> &params)
      : enumInfos(infos), paramInfos(params) {}

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

    getOperation().walk([&](ada::SubpOp subp) {
      ada::AdaParamInfo info;
      info.subpScope = subp.getMangledName();
      for (unsigned i = 0; i < subp.getNumArguments(); ++i) {
        auto typeRef = mlir::dyn_cast_or_null<mlir::FlatSymbolRefAttr>(
            subp.getArgAttr(i, "ada.type"));
        if (!typeRef)
          continue;
        mlir::BlockArgument arg = subp.getArgument(i);
        std::string paramName;
        if (auto nameLoc = mlir::dyn_cast<mlir::NameLoc>(arg.getLoc()))
          paramName = nameLoc.getName().str();
        info.params.push_back(
            {std::move(paramName), typeRef.getValue().str(), arg.getLoc(), i});
      }
      if (!info.params.empty())
        paramInfos.push_back(std::move(info));
    });
  }

  llvm::SmallVector<ada::AdaEnumInfo> &enumInfos;
  llvm::SmallVector<ada::AdaParamInfo> &paramInfos;
};
} // namespace

std::unique_ptr<mlir::Pass> mlir::ada::createAddAdaDebugInfoPass(
    llvm::SmallVector<AdaEnumInfo> &enumInfos,
    llvm::SmallVector<AdaParamInfo> &paramInfos) {
  return std::make_unique<AddAdaDebugInfoPass>(enumInfos, paramInfos);
}
