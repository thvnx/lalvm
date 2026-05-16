//===- LowerToLLVM.cpp - Lowering from Ada to LLVM ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements lowering of Ada dialect operations to the LLVM dialect.
//
//===----------------------------------------------------------------------===//

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/TypeID.h"

#include "mlir/Conversion/ArithToLLVM/ArithToLLVM.h"
#include "mlir/Conversion/ControlFlowToLLVM/ControlFlowToLLVM.h"
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVM.h"
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVMPass.h"
#include "mlir/Conversion/LLVMCommon/ConversionTarget.h"
#include "mlir/Conversion/LLVMCommon/TypeConverter.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
#include <memory>
#include <utility>

using namespace mlir;

// This file implements a single lowering pass that converts the Ada dialect
// (plus the Arith and Func dialects it relies on) directly to the LLVM dialect.
// The pass uses FullConversion, meaning every op must be lowered — no Ada ops
// are allowed to survive.
//
// Lowering chain overview:
//   ada.subp   →  func.func
//   ada.return →  func.return
//   ada.binop  →  arith.addi/subi/muli  (integers)
//                 arith.addf/subf/mulf  (floats)
//   func.func / arith.*  →  LLVM dialect  (via upstream conversion patterns)

//===----------------------------------------------------------------------===//
// AdaToLLVMLoweringPass
//===----------------------------------------------------------------------===//

namespace {
struct AdaToLLVMLoweringPass
    : public PassWrapper<AdaToLLVMLoweringPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(AdaToLLVMLoweringPass)

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<LLVM::LLVMDialect, func::FuncDialect, arith::ArithDialect>();
  }
  void runOnOperation() final;
};
} // namespace

struct NullOpLowering : public OpRewritePattern<ada::NullOp> {
  using OpRewritePattern<ada::NullOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ada::NullOp op,
                                PatternRewriter &rewriter) const final {
    rewriter.eraseOp(op);
    return success();
  }
};

struct BlockStmtOpLowering : public OpRewritePattern<ada::BlockStmtOp> {
  using OpRewritePattern<ada::BlockStmtOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ada::BlockStmtOp op,
                                PatternRewriter &rewriter) const final {
    Block &body = op.getBody().front();
    // Move all ops into the parent block, just before this op.
    rewriter.inlineBlockBefore(&body, op);
    rewriter.eraseOp(op);
    return success();
  }
};

struct ReturnOpLowering : public OpRewritePattern<ada::ReturnOp> {
  using OpRewritePattern<ada::ReturnOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ada::ReturnOp op,
                                PatternRewriter &rewriter) const final {
    // We lower "ada.return" directly to "func.return".
    rewriter.replaceOpWithNewOp<func::ReturnOp>(op, op.getOperands());
    return success();
  }
};

struct CallOpLowering : public OpRewritePattern<ada::CallOp> {
  using OpRewritePattern<ada::CallOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ada::CallOp op,
                                PatternRewriter &rewriter) const final {
    rewriter.replaceOpWithNewOp<func::CallOp>(
        op, op.getCallee(), op.getResultTypes(), op.getOperands());
    return success();
  }
};

//===----------------------------------------------------------------------===//
// AdaToLLVM RewritePatterns: Binary operations
//===----------------------------------------------------------------------===//

// Lowers ada.binop to the corresponding arith op, dispatching on the operator
// kind attribute and on integer vs. float operand type.
struct BinOpLowering : public OpRewritePattern<ada::BinOp> {
  using OpRewritePattern<ada::BinOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ada::BinOp op,
                                PatternRewriter &rewriter) const final {
    Type type = op->getOperand(0).getType();
    bool isInt = mlir::isa<mlir::IntegerType>(type);
    if (!isInt && !mlir::isa<mlir::FloatType>(type))
      return rewriter.notifyMatchFailure(op, [type](Diagnostic &diag) {
        diag << "unsupported operand type: " << type;
      });
    auto lower = [&](bool intType, auto iOp, auto fOp) {
      if (intType)
        rewriter.replaceOpWithNewOp<decltype(iOp)>(op, op->getOperands());
      else
        rewriter.replaceOpWithNewOp<decltype(fOp)>(op, op->getOperands());
    };
    switch (op.getKind()) {
    case ada::AdaBinaryOp::Plus:
      lower(isInt, arith::AddIOp{}, arith::AddFOp{});
      break;
    case ada::AdaBinaryOp::Minus:
      lower(isInt, arith::SubIOp{}, arith::SubFOp{});
      break;
    case ada::AdaBinaryOp::Mult:
      lower(isInt, arith::MulIOp{}, arith::MulFOp{});
      break;
    case ada::AdaBinaryOp::Div:
      lower(isInt, arith::DivSIOp{}, arith::DivFOp{});
      break;
    }
    return success();
  }
};

//===----------------------------------------------------------------------===//
// AdaToLLVM RewritePatterns: Subprogram operations
//===----------------------------------------------------------------------===//

// ada.subp is isomorphic to func.func at this level; we just swap the op type
// and move the region over. The upstream FuncToLLVM pass then handles the
// func.func → llvm.func conversion.
struct SubpOpLowering : public OpConversionPattern<ada::SubpOp> {
  using OpConversionPattern<ada::SubpOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::SubpOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const final {
    auto func = rewriter.create<mlir::func::FuncOp>(op.getLoc(), op.getName(),
                                                    op.getFunctionType());
    if (ArrayAttr argAttrs = op.getArgAttrsAttr())
      func.setAllArgAttrs(argAttrs);
    if (ArrayAttr resAttrs = op.getResAttrsAttr())
      func.setAllResultAttrs(resAttrs);
    rewriter.inlineRegionBefore(op.getRegion(), func.getBody(), func.end());
    rewriter.eraseOp(op);
    return success();
  }
};

void AdaToLLVMLoweringPass::runOnOperation() {
  // Two-phase ABI renaming before lowering. Both sets of ops are collected in
  // a single walk before any mutations so the parent chain is still intact.
  //
  // Phase 1 — hoist nested subprograms: LLVM does not support nested
  // functions. Each nested op is renamed with GNAT-style __ separators built
  // from the full enclosing scope chain (e.g. @inner inside @outer becomes
  // @outer__inner). Parent names at this point are still bare Ada names, so
  // the mangling matches GNAT (outer__inner, not _ada_outer__inner).
  //
  // Phase 2 — apply _ada_ prefix: library-level subprograms get the GNAT
  // _ada_ prefix. Done after hoisting so nested mangling uses bare names.
  ModuleOp module = getOperation();
  llvm::SmallVector<std::pair<Operation *, std::string>, 4> nestedSubps;
  llvm::SmallVector<Operation *, 4> librarySubps;
  // Collect all subprograms in one walk before any mutations. The walk
  // completes fully before the loops below mutate the IR (moveBefore,
  // setSymbolName), so there is no iterator invalidation.
  module.walk([&](Operation *op) {
    if (!isa<ada::SubpOp>(op))
      return;
    if (isa<ModuleOp>(op->getParentOp())) {
      librarySubps.push_back(op);
      return;
    }
    std::string name = mlir::SymbolTable::getSymbolName(op).str();
    for (Operation *p = op->getParentOp(); isa<ada::SubpOp>(p);
         p = p->getParentOp())
      name = mlir::SymbolTable::getSymbolName(p).str() + "__" + name;
    nestedSubps.emplace_back(op, std::move(name));
  });
  for (auto &[op, mangledName] : nestedSubps) {
    auto mangledAttr = mlir::StringAttr::get(module.getContext(), mangledName);
    // Save the old name before any mutation.
    std::string oldName = mlir::SymbolTable::getSymbolName(op).str();
    if (mlir::failed(
            mlir::SymbolTable::replaceAllSymbolUses(op, mangledAttr, module)))
      return signalPassFailure();
    // replaceAllSymbolUses stops at the symbol's own definition body, so
    // self-recursive calls inside 'op' are left unrenamed.  Fix that with a
    // targeted walk that crosses SymbolTable boundaries.
    auto mangledSymRef =
        mlir::FlatSymbolRefAttr::get(module.getContext(), mangledName);
    op->walk([&](ada::CallOp callOp) {
      if (callOp.getCallee() == oldName)
        callOp.setCalleeAttr(mangledSymRef);
    });
    mlir::SymbolTable::setSymbolName(op, mangledName);
    op->moveBefore(module.getBody(), module.getBody()->end());
  }
  for (Operation *op : librarySubps) {
    std::string mangledName =
        "_ada_" + mlir::SymbolTable::getSymbolName(op).str();
    auto mangledAttr = mlir::StringAttr::get(module.getContext(), mangledName);
    if (mlir::failed(
            mlir::SymbolTable::replaceAllSymbolUses(op, mangledAttr, module)))
      return signalPassFailure();
    mlir::SymbolTable::setSymbolName(op, mangledName);
  }

  // The first thing to define is the conversion target. This will define the
  // final target for this lowering. For this lowering, we are only targeting
  // the LLVM dialect.
  // LLVMConversionTarget marks all LLVM dialect ops as legal and everything
  // else (including ada.*) as illegal, driving the full conversion.
  LLVMConversionTarget target(getContext());
  target.addLegalOp<ModuleOp>();

  // LLVMTypeConverter maps MLIR types (i32, f64, …) to their LLVM equivalents.
  // It is threaded through the upstream conversion patterns that need it.
  LLVMTypeConverter typeConverter(&getContext());

  // Provide the patterns used for lowering.
  RewritePatternSet patterns(&getContext());
  // TODO: add populateSCFToControlFlowConversionPatterns once the Ada codegen
  // emits SCF ops (e.g. for if/loop statements).
  mlir::arith::populateArithToLLVMConversionPatterns(typeConverter, patterns);
  cf::populateControlFlowToLLVMConversionPatterns(typeConverter, patterns);
  populateFuncToLLVMConversionPatterns(typeConverter, patterns);

  patterns.add<NullOpLowering, BlockStmtOpLowering, ReturnOpLowering,
               CallOpLowering, SubpOpLowering, BinOpLowering>(&getContext());

  // We want to completely lower to LLVM, so we use a `FullConversion`. This
  // ensures that only legal operations will remain after the conversion.
  if (failed(applyFullConversion(module, target, std::move(patterns))))
    signalPassFailure();
}

/// Create a pass for lowering Ada dialect operations to the LLVM dialect.
std::unique_ptr<mlir::Pass> mlir::ada::createLowerToLLVMPass() {
  return std::make_unique<AdaToLLVMLoweringPass>();
}
