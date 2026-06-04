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
#include "mlir/Conversion/MemRefToLLVM/MemRefToLLVM.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
#include <memory>
#include <utility>

using namespace mlir;

// This file implements a single lowering pass that converts the Ada dialect
// (plus the Arith and Func dialects it relies on) directly to the LLVM dialect.
// The pass uses FullConversion, meaning every op must be lowered; no Ada ops
// are allowed to survive.
//
// Lowering chain overview:
//   ada.subp     ->  func.func
//   ada.return   ->  func.return
//   ada.binop    ->  arith.addi/subi/muli  (integers)
//                    arith.addf/subf/mulf  (floats)
//   func.func / arith.*  ->  LLVM dialect  (via upstream conversion patterns)

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

/// If `loc` is a NameLoc, wrap its child in a FusedLoc that carries the Ada
/// type reference `adaType` as metadata. AdaDebugInfoPass reads this to resolve
/// the correct DIType for each value, bypassing the MLIR-type-keyed cache that
/// conflates distinct Ada types sharing the same machine representation.
static mlir::Location attachAdaTypeRef(MLIRContext *ctx, mlir::Location loc,
                                       mlir::FlatSymbolRefAttr adaType) {
  auto nl = mlir::dyn_cast<mlir::NameLoc>(loc);
  if (!nl)
    return loc;
  return mlir::NameLoc::get(
      nl.getName(), mlir::FusedLoc::get(ctx, {nl.getChildLoc()},
                                        ada::DITypeRefAttr::get(ctx, adaType)));
}

// OpConversionPattern: the result type changes from !ada.qual to a bare MLIR
// type; the conversion framework must track that mapping for downstream uses.
struct ConstantOpLowering : public OpConversionPattern<ada::ConstantOp> {
  using OpConversionPattern<ada::ConstantOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::ConstantOp op, OpAdaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto typed = mlir::cast<ada::QualType>(op.getResult().getType());
    mlir::Location loc = attachAdaTypeRef(rewriter.getContext(), op.getLoc(),
                                          typed.getAdaType());
    auto newOp = rewriter.create<mlir::arith::ConstantOp>(
        loc, mlir::cast<mlir::TypedAttr>(op.getValue()));
    rewriter.replaceOp(op, newOp.getResult());
    return success();
  }
};

// OpConversionPattern: needs getTypeConverter()->convertType() for the result
// type and adaptor operands that have already been type-converted by upstream
// patterns.
struct CoerceOpLowering : public OpConversionPattern<ada::CoerceOp> {
  using OpConversionPattern<ada::CoerceOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::CoerceOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    mlir::Value input = adaptor.getInput();
    mlir::Type resultType =
        getTypeConverter()->convertType(op.getResult().getType());
    if (input.getType() == resultType) {
      rewriter.replaceOp(op, input);
      return success();
    }

    auto resultTyped = mlir::cast<ada::QualType>(op.getResult().getType());
    mlir::Location loc = attachAdaTypeRef(rewriter.getContext(), op.getLoc(),
                                          resultTyped.getAdaType());

    if (auto srcInt = mlir::dyn_cast<mlir::IntegerType>(input.getType())) {
      if (auto dstInt = mlir::dyn_cast<mlir::IntegerType>(resultType)) {
        mlir::Operation *newOp;
        if (dstInt.getWidth() < srcInt.getWidth()) {
          newOp = rewriter.create<arith::TruncIOp>(loc, resultType, input);
        } else {
          // Modular (unsigned) source types need zero-extension.
          auto inTyped = mlir::cast<ada::QualType>(op.getInput().getType());
          bool isModular = false;
          if (auto *sym = mlir::SymbolTable::lookupNearestSymbolFrom(
                  op, inTyped.getAdaType().getRootReference()))
            if (auto typeOp = mlir::dyn_cast<ada::TypeOp>(sym))
              if (auto numInfo = mlir::dyn_cast<ada::NumericTypeInfoAttr>(
                      typeOp.getTypeInfo()))
                isModular = numInfo.getModulus() != 0;
          newOp = isModular
                      ? rewriter.create<arith::ExtUIOp>(loc, resultType, input)
                      : rewriter.create<arith::ExtSIOp>(loc, resultType, input);
        }
        rewriter.replaceOp(op, newOp);
        return success();
      }
    }
    if (auto srcFloat = mlir::dyn_cast<mlir::FloatType>(input.getType())) {
      if (auto dstFloat = mlir::dyn_cast<mlir::FloatType>(resultType)) {
        mlir::Operation *newOp =
            dstFloat.getWidth() < srcFloat.getWidth()
                ? rewriter.create<arith::TruncFOp>(loc, resultType, input)
                : rewriter.create<arith::ExtFOp>(loc, resultType, input);
        rewriter.replaceOp(op, newOp);
        return success();
      }
    }
    return rewriter.notifyMatchFailure(
        op, "cross-kind conversion not yet supported");
  }
};

// OpRewritePattern: no ada.qual operands or results; plain erasure needs no
// type converter.
struct NullOpLowering : public OpRewritePattern<ada::NullOp> {
  using OpRewritePattern<ada::NullOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ada::NullOp op,
                                PatternRewriter &rewriter) const final {
    rewriter.eraseOp(op);
    return success();
  }
};

// OpConversionPattern: operands are ada.qual typed; the adaptor provides them
// already converted to bare MLIR types.
struct ReturnOpLowering : public OpConversionPattern<ada::ReturnOp> {
  using OpConversionPattern<ada::ReturnOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::ReturnOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    rewriter.replaceOpWithNewOp<func::ReturnOp>(op, adaptor.getOperands());
    return success();
  }
};

// OpConversionPattern: operands are ada.qual typed (adaptor converts them);
// result types need getTypeConverter()->convertType().
struct CallOpLowering : public OpConversionPattern<ada::CallOp> {
  using OpConversionPattern<ada::CallOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::CallOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    SmallVector<mlir::Type> resultTypes;
    for (mlir::Type t : op.getResultTypes())
      resultTypes.push_back(getTypeConverter()->convertType(t));
    rewriter.replaceOpWithNewOp<func::CallOp>(op, op.getCallee(), resultTypes,
                                              adaptor.getOperands());
    return success();
  }
};

//===----------------------------------------------------------------------===//
// AdaToLLVM RewritePatterns: Binary operations
//===----------------------------------------------------------------------===//

// Lowers ada.binop to the corresponding arith op, dispatching on the operator
// kind attribute and on integer vs. float operand type.
// OpConversionPattern: operands are ada.qual typed; the adaptor provides the
// already-converted lhs/rhs.
struct BinOpLowering : public OpConversionPattern<ada::BinOp> {
  using OpConversionPattern<ada::BinOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::BinOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    Type type = adaptor.getLhs().getType();
    bool isInt = mlir::isa<mlir::IntegerType>(type);
    if (!isInt && !mlir::isa<mlir::FloatType>(type))
      return rewriter.notifyMatchFailure(op, [type](Diagnostic &diag) {
        diag << "unsupported operand type: " << type;
      });
    auto lower = [&](bool intType, auto iOp, auto fOp) {
      if (intType)
        rewriter.replaceOpWithNewOp<decltype(iOp)>(op, adaptor.getOperands());
      else
        rewriter.replaceOpWithNewOp<decltype(fOp)>(op, adaptor.getOperands());
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
// AdaToLLVM RewritePatterns: MemRef operations on ada.qual element types
//===----------------------------------------------------------------------===//
// convertType(memref<!ada.qual<T,@s>>) returns ptr.  The standard
// LoadOpLowering/StoreOpLowering build a MemRefDescriptor and assert the
// value has struct type: they crash given a bare ptr.  The load/store
// patterns here preempt them (benefit 2 > 1) and emit bare llvm ops.

// OpConversionPattern: needs getTypeConverter()->convertType() to map the
// ada.qual element type to its LLVM equivalent.
struct AllocaAdaTypedLowering : public OpConversionPattern<ada::AllocaOp> {
  using OpConversionPattern<ada::AllocaOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::AllocaOp op, OpAdaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto typedElem = mlir::cast<ada::QualType>(
        mlir::cast<mlir::MemRefType>(op.getResult().getType())
            .getElementType());
    auto llvmElem = getTypeConverter()->convertType(typedElem.getMlirType());
    if (!llvmElem)
      return failure();
    mlir::Location loc = attachAdaTypeRef(rewriter.getContext(), op.getLoc(),
                                          typedElem.getAdaType());
    auto one = rewriter.create<LLVM::ConstantOp>(
        op.getLoc(), rewriter.getI64Type(), rewriter.getI64IntegerAttr(1));
    auto alloca = rewriter.create<LLVM::AllocaOp>(
        loc, LLVM::LLVMPointerType::get(rewriter.getContext()), llvmElem, one,
        /*alignment=*/0);
    rewriter.replaceOp(op, alloca.getRes());
    return success();
  }
};

// OpConversionPattern: the memref operand has an ada.qual element type; the
// adaptor provides the already-converted bare pointer, and getTypeConverter()
// maps the element type for the load result.
struct LoadAdaTypedLowering : public OpConversionPattern<memref::LoadOp> {
  using OpConversionPattern<memref::LoadOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(memref::LoadOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto typedElem =
        mlir::dyn_cast<ada::QualType>(op.getMemRefType().getElementType());
    if (!typedElem)
      return rewriter.notifyMatchFailure(op, "element type is not ada.qual");
    auto llvmElem = getTypeConverter()->convertType(typedElem.getMlirType());
    if (!llvmElem)
      return failure();
    rewriter.replaceOpWithNewOp<LLVM::LoadOp>(op, llvmElem,
                                              adaptor.getMemref());
    return success();
  }
};

// OpConversionPattern: the value operand is ada.qual typed and the memref is a
// bare pointer after conversion; both are provided type-converted by the
// adaptor.
struct StoreAdaTypedLowering : public OpConversionPattern<memref::StoreOp> {
  using OpConversionPattern<memref::StoreOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(memref::StoreOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    if (!mlir::isa<ada::QualType>(op.getMemRefType().getElementType()))
      return rewriter.notifyMatchFailure(op, "element type is not ada.qual");
    rewriter.replaceOpWithNewOp<LLVM::StoreOp>(op, adaptor.getValue(),
                                               adaptor.getMemref());
    return success();
  }
};

//===----------------------------------------------------------------------===//
// AdaToLLVM RewritePatterns: Subprogram operations
//===----------------------------------------------------------------------===//

// ada.subp is isomorphic to func.func at this level; we just swap the op type
// and move the region over. The upstream FuncToLLVM pass then handles the
// func.func -> llvm.func conversion.
// OpConversionPattern: parameter and result types are ada.qual and need
// getTypeConverter()->convertType(); convertRegionTypes() rewrites all value
// types inside the body in one pass, including block arguments.
struct SubpOpLowering : public OpConversionPattern<ada::SubpOp> {
  using OpConversionPattern<ada::SubpOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::SubpOp op, OpAdaptor,
                  ConversionPatternRewriter &rewriter) const override {
    mlir::FunctionType funcType = op.getFunctionType();

    // Collect Ada type refs for ada.qual input params before region move.
    SmallVector<mlir::FlatSymbolRefAttr> paramTypeRefs;
    for (mlir::Type t : funcType.getInputs()) {
      if (auto typed = dyn_cast<ada::QualType>(t))
        paramTypeRefs.push_back(typed.getAdaType());
      else
        paramTypeRefs.push_back({});
    }

    SmallVector<mlir::Type> inputTypes, resultTypes;
    for (mlir::Type t : funcType.getInputs())
      inputTypes.push_back(getTypeConverter()->convertType(t));
    for (mlir::Type t : funcType.getResults())
      resultTypes.push_back(getTypeConverter()->convertType(t));
    auto newFuncType =
        mlir::FunctionType::get(op.getContext(), inputTypes, resultTypes);
    auto func = rewriter.create<mlir::func::FuncOp>(op.getLoc(), op.getName(),
                                                    newFuncType);
    // Carry the subprogram's symbol visibility, and give private (nested)
    // subprograms internal LLVM linkage; library-level (public) subprograms
    // keep public visibility and external linkage. `func.func` visibility does
    // not by itself set linkage, so the `llvm.linkage` attribute is what makes
    // the lowered function `internal`.
    func.setVisibility(op.getVisibility());
    if (op.isPrivate())
      func->setAttr("llvm.linkage",
                    mlir::LLVM::LinkageAttr::get(
                        rewriter.getContext(), mlir::LLVM::Linkage::Internal));
    if (ArrayAttr argAttrs = op.getArgAttrsAttr())
      func.setAllArgAttrs(argAttrs);
    if (ArrayAttr resAttrs = op.getResAttrsAttr())
      func.setAllResultAttrs(resAttrs);
    // Carry the result's Ada type ref on the function location so
    // AdaDebugInfoPass can emit the subprogram's return type (DW_AT_type).
    // Mirrors the per-parameter DITypeRef attached to entry block-arg locs
    // below, but on the function's own loc: DIScopeForLLVMFuncOpPass nests this
    // FusedLoc under the DISubprogram, where the DI pass reads it back.
    // Procedures have no result and keep the plain loc (null return = void).
    if (!funcType.getResults().empty())
      if (auto typed = dyn_cast<ada::QualType>(funcType.getResults()[0]))
        func->setLoc(mlir::FusedLoc::get(
            func.getContext(), {func.getLoc()},
            ada::DITypeRefAttr::get(func.getContext(), typed.getAdaType())));
    rewriter.inlineRegionBefore(op.getRegion(), func.getBody(), func.end());
    if (mlir::failed(
            rewriter.convertRegionTypes(&func.getBody(), *getTypeConverter())))
      return mlir::failure();

    // Attach Ada type refs to entry block args for AdaDebugInfoPass.
    if (!func.getBody().empty()) {
      Block &entry = func.getBody().front();
      for (size_t i = 0;
           i < paramTypeRefs.size() && i < entry.getNumArguments(); ++i) {
        if (!paramTypeRefs[i])
          continue;
        BlockArgument arg = entry.getArgument(i);
        arg.setLoc(
            attachAdaTypeRef(op.getContext(), arg.getLoc(), paramTypeRefs[i]));
      }
    }

    rewriter.eraseOp(op);
    return success();
  }
};

void AdaToLLVMLoweringPass::runOnOperation() {
  ModuleOp module = getOperation();

  // The first thing to define is the conversion target. This will define the
  // final target for this lowering. For this lowering, we are only targeting
  // the LLVM dialect.
  // LLVMConversionTarget marks all LLVM dialect ops as legal and everything
  // else (including ada.*) as illegal, driving the full conversion.
  LLVMConversionTarget target(getContext());
  target.addLegalOp<ModuleOp>();
  target.addLegalOp<ada::TypeOp>();

  // LLVMTypeConverter maps MLIR types (i32, f64, ...) to their LLVM
  // equivalents. It is threaded through the upstream conversion patterns that
  // need it. Use bare pointer calling convention so memref<T> function
  // arguments lower to a single ptr instead of the full { ptr, ptr, i64 }
  // descriptor struct.
  mlir::LowerToLLVMOptions opts(&getContext());
  opts.useBarePtrCallConv = true;
  LLVMTypeConverter typeConverter(&getContext(), opts);
  // ada.qual<T, @sym> strips the Ada type annotation and lowers T directly.
  // e.g. !ada.qual<i32, @standard.integer> -> i32.
  typeConverter.addConversion([&](ada::QualType t) -> mlir::Type {
    return typeConverter.convertType(t.getMlirType());
  });
  // memref<!ada.qual<T,@s>> lowers to ptr (bare-ptr convention).
  // Returning std::nullopt for non-ada.qual memrefs lets the standard
  // converter run next so ordinary memrefs are unaffected.
  typeConverter.addConversion(
      [&](mlir::MemRefType t) -> std::optional<mlir::Type> {
        if (!mlir::isa<ada::QualType>(t.getElementType()))
          return std::nullopt;
        return LLVM::LLVMPointerType::get(t.getContext());
      });

  // Provide the patterns used for lowering.
  RewritePatternSet patterns(&getContext());
  // TODO: add populateSCFToControlFlowConversionPatterns once the Ada codegen
  // emits SCF ops (e.g. for if/loop statements).
  mlir::arith::populateArithToLLVMConversionPatterns(typeConverter, patterns);
  cf::populateControlFlowToLLVMConversionPatterns(typeConverter, patterns);
  populateFuncToLLVMConversionPatterns(typeConverter, patterns);
  populateFinalizeMemRefToLLVMConversionPatterns(typeConverter, patterns);

  patterns.add<NullOpLowering>(&getContext());
  patterns.add<ReturnOpLowering, CallOpLowering, BinOpLowering, SubpOpLowering,
               ConstantOpLowering, CoerceOpLowering>(typeConverter,
                                                     &getContext());
  patterns
      .add<AllocaAdaTypedLowering, LoadAdaTypedLowering, StoreAdaTypedLowering>(
          typeConverter, &getContext(), PatternBenefit(2));

  // We want to completely lower to LLVM, so we use a `FullConversion`. This
  // ensures that only legal operations will remain after the conversion.
  if (failed(applyFullConversion(module, target, std::move(patterns))))
    signalPassFailure();
}

/// Create a pass for lowering Ada dialect operations to the LLVM dialect.
std::unique_ptr<mlir::Pass> mlir::ada::createLowerToLLVMPass() {
  return std::make_unique<AdaToLLVMLoweringPass>();
}
