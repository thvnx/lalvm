//===- FinalizeAdaObject.cpp - Emit LLVM debug intrinsics from NameLoc ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass emits LLVM debug intrinsics for Ada objects and parameters, and
// collects Ada enum type metadata from surviving `ada.type` ops into the
// `enumInfos` out-parameter for use by `attachAdaDebugInfo` in lalvm.cpp.
// It runs after DIScopeForLLVMFuncOpPass so that DISubprogramAttr is
// available on each llvm.func.
//
// Objects are identified by their NameLoc, set by MLIRGen on:
//   - memref.alloca (ObjectDecl): lowered to llvm.alloca
//   - arith.constant init expr (ObjectDecl with init): lowered to
//     llvm.mlir.constant (if mem2reg promotes the alloca)
//   - arith.constant (NumberDecl): lowered to llvm.mlir.constant
//   - llvm.func entry block args (parameters): NameLoc set by mlirGenSubpBody
//
// Dispatch strategy:
//   llvm.alloca with NameLoc             -> DW_TAG_variable + dbg.declare
//   other op with NameLoc, no alloca     -> DW_TAG_variable + dbg.value
//   scalar entry block arg with NameLoc  -> DW_TAG_formal_parameter + dbg.value
//   ptr entry block arg with NameLoc     -> DW_TAG_formal_parameter +
//   dbg.declare
//
// When an llvm.alloca with NameLoc("x") exists in a function, scalar ops with
// the same NameLoc("x") are suppressed to avoid duplicate debug entries.
//
// ada.type ops survive LowerToLLVM (marked legal) so that ptr parameters can
// recover the element type via the "ada.type" arg_attr. They are dropped
// silently by AdaToLLVMIRTranslation after this pass.
//
//===----------------------------------------------------------------------===//

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/Dialect/LLVMIR/LLVMAttrs.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/Path.h"

using namespace mlir;

namespace {

static LLVM::DIBasicTypeAttr makeDIIntType(MLIRContext *ctx,
                                           IntegerType intType) {
  unsigned w = intType.getWidth();
  unsigned enc =
      (w == 1) ? llvm::dwarf::DW_ATE_boolean : llvm::dwarf::DW_ATE_signed;
  unsigned sizeInBits = llvm::alignTo(w, 8);
  return LLVM::DIBasicTypeAttr::get(ctx, llvm::dwarf::DW_TAG_base_type,
                                    ("integer_" + llvm::Twine(w)).str(),
                                    sizeInBits, enc);
}

static std::pair<LLVM::DIFileAttr, unsigned>
getFileAndLine(MLIRContext *ctx, Location loc,
               LLVM::DISubprogramAttr subprogram) {
  if (auto flc = dyn_cast<FileLineColRange>(loc)) {
    StringRef filePath = flc.getFilename().getValue();
    return {LLVM::DIFileAttr::get(ctx, llvm::sys::path::filename(filePath),
                                  llvm::sys::path::parent_path(filePath)),
            flc.getStartLine()};
  }
  return {subprogram.getFile(), 0};
}

// Returns the DISubprogramAttr attached to `op` by DIScopeForLLVMFuncOpPass,
// or a null attr if the location is not a FusedLoc with DISubprogramAttr
// metadata (i.e. the function has no debug info).
static LLVM::DISubprogramAttr getSubprogram(Operation *op) {
  auto fl = dyn_cast<FusedLoc>(op->getLoc());
  if (!fl)
    return {};
  return dyn_cast_or_null<LLVM::DISubprogramAttr>(fl.getMetadata());
}

struct FinalizeAdaObjectPass
    : public PassWrapper<FinalizeAdaObjectPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(FinalizeAdaObjectPass)

  explicit FinalizeAdaObjectPass(llvm::SmallVector<ada::AdaEnumInfo> &infos)
      : enumInfos(infos) {}

  void runOnOperation() final {
    MLIRContext *ctx = &getContext();
    ModuleOp module = getOperation();

    // Pass 1: record (func, name) pairs for all llvm.alloca ops with NameLoc.
    // Used to suppress scalar debug entries when an alloca already tracks the
    // variable with dbg.declare.
    using FuncNamePair = std::pair<Operation *, StringAttr>;
    llvm::DenseSet<FuncNamePair> allocaNames;
    module.walk([&](LLVM::AllocaOp op) {
      auto nl = dyn_cast<NameLoc>(op.getLoc());
      if (!nl)
        return;
      auto func = op->getParentOfType<LLVM::LLVMFuncOp>();
      if (func)
        allocaNames.insert({func, nl.getName()});
    });

    // Pass 2: collect debug entries.
    struct DebugEntry {
      Operation *op;
      LLVM::DISubprogramAttr subprogram;
      StringAttr name;
      Location innerLoc;
      bool isDeclare; // alloca: dbg.declare; scalar: dbg.value
    };
    llvm::SmallVector<DebugEntry> entries;

    module.walk([&](Operation *op) {
      auto nl = dyn_cast<NameLoc>(op->getLoc());
      if (!nl)
        return;

      auto func = op->getParentOfType<LLVM::LLVMFuncOp>();
      if (!func)
        return;

      auto subprogram = getSubprogram(func);
      if (!subprogram)
        return;

      bool isAlloca = isa<LLVM::AllocaOp>(op);
      if (!isAlloca) {
        // Skip scalar ops when an alloca with the same name exists.
        if (allocaNames.contains({func, nl.getName()}))
          return;
        // Must produce exactly one result to emit dbg.value.
        if (op->getNumResults() != 1)
          return;
      }

      entries.push_back(
          {op, subprogram, nl.getName(), nl.getChildLoc(), isAlloca});
    });

    // Pass 3: emit debug intrinsics.
    for (auto &entry : entries) {
      auto [fileAttr, line] =
          getFileAndLine(ctx, entry.innerLoc, entry.subprogram);

      mlir::Type elemType = entry.isDeclare
                                ? cast<LLVM::AllocaOp>(entry.op).getElemType()
                                : entry.op->getResult(0).getType();
      auto intType = dyn_cast<IntegerType>(elemType);
      if (!intType)
        continue; // silently skip floats and other types for now

      auto diType = makeDIIntType(ctx, intType);
      auto varInfo = LLVM::DILocalVariableAttr::get(
          entry.subprogram, entry.name, fileAttr, line,
          /*arg=*/0, /*alignInBits=*/0, diType, LLVM::DIFlags::Zero);

      OpBuilder builder(entry.op);
      builder.setInsertionPointAfter(entry.op);
      if (entry.isDeclare)
        LLVM::DbgDeclareOp::create(builder, entry.op->getLoc(),
                                   cast<LLVM::AllocaOp>(entry.op).getRes(),
                                   varInfo);
      else
        LLVM::DbgValueOp::create(builder, entry.op->getLoc(),
                                 entry.op->getResult(0), varInfo);
    }

    // Pass 4: emit DW_TAG_formal_parameter intrinsics for llvm.func entry
    // block args with NameLoc (Ada parameters).
    //
    // Pre-build a cache to avoid O(M) symbol-table scans per ptr parameter.
    // Also collect enum type metadata into enumInfos for attachAdaDebugInfo.
    llvm::DenseMap<StringAttr, ada::TypeOp> typeOpCache;
    module.walk([&](ada::TypeOp typeOp) {
      StringAttr symName = typeOp.getSymNameAttr();
      typeOpCache[symName] = typeOp;

      auto enumInfo = dyn_cast<ada::EnumTypeInfoAttr>(typeOp.getTypeInfo());
      if (!enumInfo)
        return;
      auto intType = dyn_cast<IntegerType>(typeOp.getMlirType());
      if (!intType)
        return;

      llvm::SmallVector<std::string> names;
      for (StringRef name : enumInfo.getNames())
        names.push_back(name.str());
      llvm::SmallVector<int64_t> values(enumInfo.getValues().begin(),
                                        enumInfo.getValues().end());

      std::optional<std::string> subpScope;
      if (auto func = typeOp->getParentOfType<LLVM::LLVMFuncOp>())
        subpScope = func.getName().str();

      enumInfos.push_back({symName.getValue().str(), intType.getWidth(),
                           typeOp.getLoc(), std::move(names), std::move(values),
                           std::move(subpScope)});
    });

    OpBuilder builder(ctx);
    module.walk([&](LLVM::LLVMFuncOp func) {
      if (func.getBody().empty())
        return;
      auto subprogram = getSubprogram(func);
      if (!subprogram)
        return;

      Block &entry = func.getBody().front();
      builder.setInsertionPointToStart(&entry);

      for (BlockArgument arg : entry.getArguments()) {
        auto nl = dyn_cast<NameLoc>(arg.getLoc());
        if (!nl)
          continue;

        unsigned argIdx = arg.getArgNumber();
        unsigned argNum = argIdx + 1; // 1-based for parameters
        auto [fileAttr, line] =
            getFileAndLine(ctx, nl.getChildLoc(), subprogram);

        // Resolve the DI type and declare-vs-value mode for this parameter.
        LLVM::DITypeAttr diType;
        bool isDeclare = false;
        Type argType = arg.getType();
        if (auto intType = dyn_cast<IntegerType>(argType)) {
          // Scalar `in` parameter: dbg.value at function entry.
          diType = makeDIIntType(ctx, intType);
        } else if (isa<LLVM::LLVMPointerType>(argType)) {
          // Reference `in out`/`out` parameter: dbg.declare.
          // Recover the element type from the "ada.type" arg_attr.
          isDeclare = true;
          auto typeRef = dyn_cast_or_null<FlatSymbolRefAttr>(
              func.getArgAttr(argIdx, "ada.type"));
          if (!typeRef) {
            func.emitWarning("reference parameter '")
                << nl.getName().getValue()
                << "' has no ada.type attr; skipping debug info";
            continue;
          }
          auto it = typeOpCache.find(typeRef.getRootReference());
          if (it == typeOpCache.end())
            continue;
          auto intElemType = dyn_cast<IntegerType>(it->second.getMlirType());
          if (!intElemType)
            continue; // silently skip floats and other unsupported types
          diType = makeDIIntType(ctx, intElemType);
        } else {
          continue;
        }

        auto varInfo = LLVM::DILocalVariableAttr::get(
            subprogram, nl.getName(), fileAttr, line, argNum,
            /*alignInBits=*/0, diType, LLVM::DIFlags::Zero);
        if (isDeclare)
          LLVM::DbgDeclareOp::create(builder, nl, arg, varInfo);
        else
          LLVM::DbgValueOp::create(builder, nl, arg, varInfo);
      }
    });
  }

  llvm::SmallVector<ada::AdaEnumInfo> &enumInfos;
};
} // namespace

std::unique_ptr<mlir::Pass> mlir::ada::createFinalizeAdaObjectPass(
    llvm::SmallVector<AdaEnumInfo> &enumInfos) {
  return std::make_unique<FinalizeAdaObjectPass>(enumInfos);
}
