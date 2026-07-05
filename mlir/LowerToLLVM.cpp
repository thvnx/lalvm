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
#include "mlir/Conversion/SCFToControlFlow/SCFToControlFlow.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlow.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/FunctionCallUtils.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
#include "llvm/Support/Path.h"
#include <memory>
#include <utility>

using namespace mlir;

// This file implements a single lowering pass that converts the Ada dialect
// (plus the Arith and Func dialects it relies on) directly to the LLVM dialect.
// The pass uses FullConversion, meaning every op must be lowered; no Ada ops
// are allowed to survive.
//
// Each Ada op is lowered by its own `*OpLowering` pattern (see below), mostly
// onto the Func, Arith, and MemRef dialects, which upstream conversion patterns
// then lower the rest of the way to the LLVM dialect.

//===----------------------------------------------------------------------===//
// AdaToLLVMLoweringPass
//===----------------------------------------------------------------------===//

namespace {
struct AdaToLLVMLoweringPass
    : public PassWrapper<AdaToLLVMLoweringPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(AdaToLLVMLoweringPass)

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<LLVM::LLVMDialect, func::FuncDialect, arith::ArithDialect,
                    cf::ControlFlowDialect>();
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

/// Build an `arith.constant` of integer `type` holding `value`.
static mlir::Value constInt(mlir::ConversionPatternRewriter &rewriter,
                            mlir::Location loc, mlir::Type type,
                            int64_t value) {
  return rewriter.create<arith::ConstantOp>(
      loc, rewriter.getIntegerAttr(type, value));
}
static mlir::Value constInt(mlir::ConversionPatternRewriter &rewriter,
                            mlir::Location loc, mlir::Type type,
                            const llvm::APInt &value) {
  return rewriter.create<arith::ConstantOp>(
      loc, rewriter.getIntegerAttr(type, value));
}

/// Extract field `index` from an aggregate (struct) value.
static mlir::Value extractField(mlir::ConversionPatternRewriter &rewriter,
                                mlir::Location loc, mlir::Value agg,
                                int64_t index) {
  return rewriter.create<LLVM::ExtractValueOp>(loc, agg,
                                               llvm::ArrayRef<int64_t>{index});
}

/// The modulus of the Ada type named by `qual`, or null when it is not modular.
/// The modulus is a base-type property (a subtype's `int_info` records only its
/// range), so walk `base` links from `from` until a nonzero modulus or the
/// chain ends.
static mlir::IntegerAttr getModularModulus(mlir::Operation *from,
                                           ada::QualType qual) {
  auto typeOp = mlir::dyn_cast_or_null<ada::TypeOp>(
      mlir::SymbolTable::lookupNearestSymbolFrom(
          from, qual.getAdaType().getRootReference()));
  while (typeOp) {
    auto intInfo = mlir::dyn_cast_or_null<ada::IntegerTypeInfoAttr>(
        typeOp.getTypeInfoAttr());
    if (intInfo && intInfo.getModulus())
      return intInfo.getModulus();
    auto baseAttr = typeOp.getBaseAttr();
    if (!baseAttr)
      break;
    typeOp = mlir::dyn_cast_or_null<ada::TypeOp>(
        mlir::SymbolTable::lookupNearestSymbolFrom(typeOp, baseAttr));
  }
  return {};
}

/// Whether the Ada type named by `qual` is modular (its arithmetic wraps,
/// @rm{3-5-4}), so values are unsigned.
static bool isModularQualType(mlir::Operation *from, ada::QualType qual) {
  return static_cast<bool>(getModularModulus(from, qual));
}

/// Whether ordering comparisons on the Ada type named by `qual` are unsigned.
/// Signed integers order as signed; modular types (@rm{3-5-4}) and enumerations
/// (Boolean included, ordered by non-negative position) are unsigned. The kind
/// lives on the root type, so walk `base` links (a subtype records only its
/// range) and inspect the root's `type_info`.
static bool isUnsignedOrderQualType(mlir::Operation *from, ada::QualType qual) {
  auto typeOp = mlir::dyn_cast_or_null<ada::TypeOp>(
      mlir::SymbolTable::lookupNearestSymbolFrom(
          from, qual.getAdaType().getRootReference()));
  ada::TypeOp root;
  while (typeOp) {
    root = typeOp;
    auto baseAttr = typeOp.getBaseAttr();
    if (!baseAttr)
      break;
    typeOp = mlir::dyn_cast_or_null<ada::TypeOp>(
        mlir::SymbolTable::lookupNearestSymbolFrom(typeOp, baseAttr));
  }
  if (!root)
    return false;
  if (auto intInfo = mlir::dyn_cast_or_null<ada::IntegerTypeInfoAttr>(
          root.getTypeInfoAttr()))
    return static_cast<bool>(intInfo.getModulus());
  return mlir::isa_and_nonnull<ada::EnumTypeInfoAttr>(root.getTypeInfoAttr());
}

/// Address of a private, NUL-terminated constant holding `fileName`, for the
/// `file` argument of the runtime raise. lalvm compiles one source unit per
/// run, so all checks share a single `@lalvm.file` global, created on first
/// use and reused after.
static mlir::Value emitFileNamePtr(mlir::ConversionPatternRewriter &rewriter,
                                   mlir::Location loc, mlir::ModuleOp module,
                                   llvm::StringRef fileName) {
  MLIRContext *ctx = rewriter.getContext();
  auto ptrTy = LLVM::LLVMPointerType::get(ctx);

  auto global = module.lookupSymbol<LLVM::GlobalOp>("lalvm.file");
  if (!global) {
    std::string data = fileName.str();
    data.push_back('\0');
    auto arrTy =
        LLVM::LLVMArrayType::get(mlir::IntegerType::get(ctx, 8), data.size());
    OpBuilder::InsertionGuard guard(rewriter);
    rewriter.setInsertionPointToStart(module.getBody());
    global = rewriter.create<LLVM::GlobalOp>(
        loc, arrTy, /*isConstant=*/true, LLVM::Linkage::Private, "lalvm.file",
        rewriter.getStringAttr(data), /*alignment=*/0);
  }
  mlir::Value base =
      rewriter.create<LLVM::AddressOfOp>(loc, ptrTy, global.getSymNameAttr());
  return rewriter.create<LLVM::GEPOp>(loc, ptrTy, global.getGlobalType(), base,
                                      llvm::ArrayRef<LLVM::GEPArg>{0, 0});
}

/// Emit a Constraint_Check trap (@rm{11-5}): split the current block, build a
/// `raise` block that calls the GNAT runtime `fnName(file, line)` and is
/// unreachable, and branch there when `cond` is true, otherwise fall through to
/// the continuation. `cond` must already be materialized in the current block.
/// On return the rewriter is positioned at the start of the continuation block,
/// so a caller can keep emitting on the live (non-raising) path or chain a
/// second check. Returns failure if the runtime function cannot be declared.
static mlir::LogicalResult
emitConstraintRaise(mlir::ConversionPatternRewriter &rewriter,
                    mlir::Location loc, mlir::ModuleOp module, mlir::Value cond,
                    llvm::StringRef fnName) {
  auto ptrTy = LLVM::LLVMPointerType::get(rewriter.getContext());
  mlir::Type i32Ty = rewriter.getI32Type();
  auto fn =
      LLVM::lookupOrCreateFn(rewriter, module, fnName, {ptrTy, i32Ty},
                             LLVM::LLVMVoidType::get(rewriter.getContext()));
  if (mlir::failed(fn))
    return mlir::failure();

  // Split the block at the check; later uses of the value move to `cont`.
  mlir::Block *opBlock = rewriter.getInsertionBlock();
  mlir::Block *cont =
      rewriter.splitBlock(opBlock, rewriter.getInsertionPoint());

  // Raise block: fnName(file, line); unreachable.
  mlir::Block *raise = rewriter.createBlock(cont);
  llvm::StringRef fileName;
  unsigned line = 0;
  if (auto flc = mlir::dyn_cast<mlir::FileLineColLoc>(loc)) {
    fileName = llvm::sys::path::filename(flc.getFilename().getValue());
    line = flc.getLine();
  }
  mlir::Value file = emitFileNamePtr(rewriter, loc, module, fileName);
  mlir::Value lineVal = rewriter.create<LLVM::ConstantOp>(
      loc, i32Ty, rewriter.getI32IntegerAttr(line));
  rewriter.create<LLVM::CallOp>(loc, *fn, mlir::ValueRange{file, lineVal});
  rewriter.create<LLVM::UnreachableOp>(loc);

  // Test in the original block: bad -> raise, else -> continue.
  rewriter.setInsertionPointToEnd(opBlock);
  rewriter.create<LLVM::CondBrOp>(loc, cond, raise, cont);

  // Resume on the live path so the caller can emit the guarded operation (or
  // chain another check) after the branch.
  rewriter.setInsertionPointToStart(cont);
  return mlir::success();
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
          // Modular (unsigned) source types need zero-extension; signed types
          // need sign-extension.
          auto inTyped = mlir::cast<ada::QualType>(op.getInput().getType());
          newOp = isModularQualType(op, inTyped)
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

// OpConversionPattern: the `!ada.range` result lowers to an LLVM struct, built
// `undef` + one `insertvalue` per bound. The adaptor gives the bounds already
// converted to bare machine types.
struct RangeOpLowering : public OpConversionPattern<ada::RangeOp> {
  using OpConversionPattern<ada::RangeOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::RangeOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    mlir::Type structTy =
        getTypeConverter()->convertType(op.getResult().getType());
    if (!structTy)
      return rewriter.notifyMatchFailure(op, "could not convert !ada.range");
    mlir::Location loc = op.getLoc();
    auto subtypeSym = mlir::cast<ada::RangeType>(op.getResult().getType())
                          .getConstrainedType();
    mlir::Value agg = rewriter.create<LLVM::UndefOp>(loc, structTy);
    auto lo = rewriter.create<LLVM::InsertValueOp>(loc, agg, adaptor.getLow(),
                                                   llvm::ArrayRef<int64_t>{0});
    auto hi = rewriter.create<LLVM::InsertValueOp>(loc, lo, adaptor.getHigh(),
                                                   llvm::ArrayRef<int64_t>{1});
    // Tag a dynamic (non-constant) bound with its subtype symbol so
    // AdaDebugInfoPass can attach an artificial DI variable for the subrange
    // type's bound; the insertvalue position (0 low, 1 high) names which bound.
    // The marker rides on the value's location, like the other Ada DI metadata.
    auto *ctx = rewriter.getContext();
    auto isConst = [](mlir::Value v) {
      return v.getDefiningOp<arith::ConstantOp>() ||
             v.getDefiningOp<LLVM::ConstantOp>();
    };
    auto markDyn = [&](LLVM::InsertValueOp iv) {
      iv->setLoc(mlir::FusedLoc::get(
          ctx, {iv->getLoc()}, ada::DIDynBoundAttr::get(ctx, subtypeSym)));
    };
    if (!isConst(adaptor.getLow()))
      markDyn(lo);
    if (!isConst(adaptor.getHigh()))
      markDyn(hi);
    rewriter.replaceOp(op, hi.getResult());
    return success();
  }
};

// OpConversionPattern: lowers the Constraint_Check (@rm{11-5}) to a compare
// against the bound pair and a conditional branch to a raise block that calls
// the GNAT runtime and is unreachable. Type-preserving: the checked value
// flows through unchanged into the continuation.
struct RangeCheckOpLowering : public OpConversionPattern<ada::RangeCheckOp> {
  using OpConversionPattern<ada::RangeCheckOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::RangeCheckOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    mlir::Location loc = op.getLoc();
    auto module = op->getParentOfType<mlir::ModuleOp>();
    mlir::Value value = adaptor.getValue();

    // Bounds: read the two fields of the lowered descriptor. `extractvalue` of
    // the producing `insertvalue` folds away under optimization.
    mlir::Value desc = adaptor.getRange();
    mlir::Value lo = extractField(rewriter, loc, desc, 0);
    mlir::Value hi = extractField(rewriter, loc, desc, 1);

    // Out of range when below the low bound or above the high bound.
    mlir::Value below, above;
    if (mlir::isa<mlir::FloatType>(value.getType())) {
      // Unordered predicates so a NaN value (in no range) raises: `NaN ult lo`
      // is true, whereas the ordered `olt` would let it pass.
      below = rewriter.create<LLVM::FCmpOp>(loc, LLVM::FCmpPredicate::ult,
                                            value, lo);
      above = rewriter.create<LLVM::FCmpOp>(loc, LLVM::FCmpPredicate::ugt,
                                            value, hi);
    } else {
      bool mod = isModularQualType(
          op, mlir::cast<ada::QualType>(op.getValue().getType()));
      below = rewriter.create<LLVM::ICmpOp>(
          loc, mod ? LLVM::ICmpPredicate::ult : LLVM::ICmpPredicate::slt, value,
          lo);
      above = rewriter.create<LLVM::ICmpOp>(
          loc, mod ? LLVM::ICmpPredicate::ugt : LLVM::ICmpPredicate::sgt, value,
          hi);
    }
    mlir::Value bad = rewriter.create<LLVM::OrOp>(loc, below, above);

    if (mlir::failed(emitConstraintRaise(rewriter, loc, module, bad,
                                         "__gnat_rcheck_CE_Range_Check")))
      return mlir::failure();

    rewriter.replaceOp(op, value);
    return success();
  }
};

// Lowers ada.attr (`'First`/`'Last`) to an `extractvalue` of the lowered range
// descriptor: field 0 is the low bound (`'First`), field 1 the high
// (`'Last`). When the descriptor comes from a visible `ada.range`, the
// `extractvalue` of the producing `insertvalue` folds away under optimization,
// so a static bound collapses to its constant.
struct AttrOpLowering : public OpConversionPattern<ada::AttrOp> {
  using OpConversionPattern<ada::AttrOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::AttrOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    int64_t field = op.getName() == "first" ? 0 : 1;
    rewriter.replaceOpWithNewOp<LLVM::ExtractValueOp>(
        op, adaptor.getRange(), llvm::ArrayRef<int64_t>{field});
    return success();
  }
};

// OpRewritePattern: no ada.qual operands or results; plain erasure needs no
// type converter.
//
// @todo Erasing drops the `null;` source location, the only anchor for that
// line in the IR. To let a debugger break on a bare `null;` line (as GNAT
// does), lower it to a location-carrying nop instead of erasing it.
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

// Value of a checked signed-integer arithmetic tree when it is a compile-time
// constant that does not overflow, else nullopt. After `mem2reg` the promoted
// locals are constants, so such an operation provably cannot raise and needs no
// overflow check -- the plain arith op is emitted instead and LLVM folds it,
// matching GNAT, which folds e.g. `X + Y` to a literal when X and Y have known
// values. Only overflow-flagged (signed, no modulus) `+`/`-`/`*` are evaluated,
// with exact two's-complement `APInt` arithmetic.
static std::optional<llvm::APInt> evalConstInt(mlir::Value v) {
  if (auto c = v.getDefiningOp<ada::ConstantOp>())
    if (auto ia = mlir::dyn_cast<mlir::IntegerAttr>(c.getValue()))
      return ia.getValue();
  if (auto c = v.getDefiningOp<arith::ConstantOp>())
    if (auto ia = mlir::dyn_cast<mlir::IntegerAttr>(c.getValue()))
      return ia.getValue();
  auto bin = v.getDefiningOp<ada::BinOp>();
  if (!bin)
    return std::nullopt;
  ada::AdaChecksAttr checks = bin.getChecksAttr();
  if (!checks ||
      !ada::bitEnumContainsAny(checks.getValue(), ada::AdaChecks::Overflow))
    return std::nullopt;
  std::optional<llvm::APInt> l = evalConstInt(bin.getLhs());
  std::optional<llvm::APInt> r = evalConstInt(bin.getRhs());
  if (!l || !r)
    return std::nullopt;
  llvm::APInt result;
  bool overflow = false;
  switch (bin.getKind()) {
  case ada::AdaBinaryOp::Plus:
    result = l->sadd_ov(*r, overflow);
    break;
  case ada::AdaBinaryOp::Minus:
    result = l->ssub_ov(*r, overflow);
    break;
  case ada::AdaBinaryOp::Mult:
    result = l->smul_ov(*r, overflow);
    break;
  default:
    return std::nullopt;
  }
  return overflow ? std::nullopt : std::optional<llvm::APInt>(result);
}

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
    if (!mlir::isa<mlir::IntegerType>(type) &&
        !mlir::isa<mlir::FloatType>(type))
      return rewriter.notifyMatchFailure(op, [type](Diagnostic &diag) {
        diag << "unsupported operand type: " << type;
      });

    // Try each specialized lowering in turn; a helper returns nullopt when it
    // does not apply, so the op falls through to the plain arith lowering.
    if (auto r = lowerCheckedArithmetic(op, adaptor, rewriter))
      return *r;
    if (auto r = lowerModularArithmetic(op, adaptor, rewriter))
      return *r;
    lowerPlainArithmetic(op, adaptor, rewriter);
    return success();
  }

private:
  // Integer arithmetic that can trap a Constraint_Error: signed +/-/* with the
  // overflow flag, and integer division. Returns nullopt for anything else
  // (floats, modular +/-/*, unflagged or const-foldable +/-/*), leaving it to a
  // later lowering.
  std::optional<LogicalResult>
  lowerCheckedArithmetic(ada::BinOp op, OpAdaptor adaptor,
                         ConversionPatternRewriter &rewriter) const {
    Type type = adaptor.getLhs().getType();
    if (!mlir::isa<mlir::IntegerType>(type))
      return std::nullopt;
    ada::AdaBinaryOp kind = op.getKind();
    ada::AdaChecks checks = {};
    if (ada::AdaChecksAttr a = op.getChecksAttr())
      checks = a.getValue();

    // Signed +/-/* carrying the overflow flag lower to the LLVM checked
    // intrinsic and trap to the GNAT runtime on overflow (@rm{4-5}). An
    // unflagged or provably-safe compile-time-constant +/-/* is left to the
    // plain lowering, which LLVM folds.
    if (ada::bitEnumContainsAny(checks, ada::AdaChecks::Overflow) &&
        !evalConstInt(op.getResult())) {
      mlir::Location loc = op.getLoc();
      auto module = op->getParentOfType<mlir::ModuleOp>();
      auto st = LLVM::LLVMStructType::getLiteral(rewriter.getContext(),
                                                 {type, rewriter.getI1Type()});
      mlir::Value lhs = adaptor.getLhs(), rhs = adaptor.getRhs();
      mlir::Value wo;
      switch (kind) {
      case ada::AdaBinaryOp::Plus:
        wo = rewriter.create<LLVM::SAddWithOverflowOp>(loc, st, lhs, rhs);
        break;
      case ada::AdaBinaryOp::Minus:
        wo = rewriter.create<LLVM::SSubWithOverflowOp>(loc, st, lhs, rhs);
        break;
      case ada::AdaBinaryOp::Mult:
        wo = rewriter.create<LLVM::SMulWithOverflowOp>(loc, st, lhs, rhs);
        break;
      default:
        return rewriter.notifyMatchFailure(op, "overflow check on non-+-* op");
      }
      mlir::Value res = extractField(rewriter, loc, wo, 0);
      mlir::Value ovf = extractField(rewriter, loc, wo, 1);
      if (mlir::failed(emitConstraintRaise(rewriter, loc, module, ovf,
                                           "__gnat_rcheck_CE_Overflow_Check")))
        return mlir::failure();
      rewriter.replaceOp(op, res);
      return success();
    }

    // Integer division (@rm{11-5}, @rm{4-5}). Modular division is unsigned, all
    // other integer division signed; neither exceeds the modulus/range, so no
    // reduction is needed. When the `division` check is set, guard the divide
    // with a zero-divisor precheck (and, for signed division, the
    // `Integer'First / -1` overflow precheck), since an LLVM divide by zero is
    // undefined and there is no checked-division intrinsic. The prechecks must
    // precede the divide, which is emitted on the live path that
    // `emitConstraintRaise` leaves us on.
    if (kind == ada::AdaBinaryOp::Div) {
      mlir::Location loc = op.getLoc();
      mlir::Value lhs = adaptor.getLhs(), rhs = adaptor.getRhs();
      bool modular = static_cast<bool>(getModularModulus(
          op, mlir::cast<ada::QualType>(op.getResult().getType())));

      if (ada::bitEnumContainsAny(checks, ada::AdaChecks::Division)) {
        auto module = op->getParentOfType<mlir::ModuleOp>();
        unsigned w = mlir::cast<mlir::IntegerType>(type).getWidth();

        // Zero divisor raises Constraint_Error, for any integer `/`.
        mlir::Value zero = constInt(rewriter, loc, type, 0);
        mlir::Value isZero = rewriter.create<arith::CmpIOp>(
            loc, arith::CmpIPredicate::eq, rhs, zero);
        if (mlir::failed(
                emitConstraintRaise(rewriter, loc, module, isZero,
                                    "__gnat_rcheck_CE_Divide_By_Zero")))
          return mlir::failure();

        // `Integer'First / -1` overflows the signed range (its result does not
        // fit); modular division wraps and never overflows.
        if (!modular) {
          mlir::Value intMin =
              constInt(rewriter, loc, type, llvm::APInt::getSignedMinValue(w));
          mlir::Value negOne =
              constInt(rewriter, loc, type, llvm::APInt::getAllOnes(w));
          mlir::Value lhsIsMin = rewriter.create<arith::CmpIOp>(
              loc, arith::CmpIPredicate::eq, lhs, intMin);
          mlir::Value rhsIsNegOne = rewriter.create<arith::CmpIOp>(
              loc, arith::CmpIPredicate::eq, rhs, negOne);
          mlir::Value ovf =
              rewriter.create<arith::AndIOp>(loc, lhsIsMin, rhsIsNegOne);
          if (mlir::failed(
                  emitConstraintRaise(rewriter, loc, module, ovf,
                                      "__gnat_rcheck_CE_Overflow_Check")))
            return mlir::failure();
        }
      }

      if (modular)
        rewriter.replaceOpWithNewOp<arith::DivUIOp>(op, lhs, rhs);
      else
        rewriter.replaceOpWithNewOp<arith::DivSIOp>(op, lhs, rhs);
      return success();
    }

    return std::nullopt;
  }

  // Modular arithmetic (@rm{3-5-4}). Values are unsigned, and the stored
  // integer width byte-rounds up to a power of two, so the width's own
  // wraparound realizes the modulus only when it is exactly 2**width (the
  // common `Interfaces.Unsigned_*` / `mod 256` families). Returns nullopt for a
  // non-modular op and for that 2**width case, letting the plain lowering emit
  // the bare arith op whose wraparound is already exact.
  std::optional<LogicalResult>
  lowerModularArithmetic(ada::BinOp op, OpAdaptor adaptor,
                         ConversionPatternRewriter &rewriter) const {
    Type type = adaptor.getLhs().getType();
    if (!mlir::isa<mlir::IntegerType>(type))
      return std::nullopt;
    mlir::IntegerAttr modAttr = getModularModulus(
        op, mlir::cast<ada::QualType>(op.getResult().getType()));
    if (!modAttr)
      return std::nullopt;
    ada::AdaBinaryOp kind = op.getKind();
    mlir::Location loc = op.getLoc();
    mlir::Value lhs = adaptor.getLhs(), rhs = adaptor.getRhs();

    // For any other modulus, +/-/* must be explicitly reduced; the width's
    // wraparound is exact only at modulus == 2**width.
    llvm::APInt m = modAttr.getValue();
    unsigned w = mlir::cast<mlir::IntegerType>(type).getWidth();

    if (m.isPowerOf2() && m.logBase2() == w)
      return std::nullopt; // width-based wraparound is already exact

    if (m.isPowerOf2()) {
      // Power-of-two modulus narrower than the width: mask the low k bits.
      // Correct for +/-/* alike, since 2**w is a multiple of 2**k.
      unsigned k = m.logBase2();
      mlir::Value base;
      switch (kind) {
      case ada::AdaBinaryOp::Plus:
        base = rewriter.create<arith::AddIOp>(loc, lhs, rhs);
        break;
      case ada::AdaBinaryOp::Minus:
        base = rewriter.create<arith::SubIOp>(loc, lhs, rhs);
        break;
      default: // Mult
        base = rewriter.create<arith::MulIOp>(loc, lhs, rhs);
        break;
      }
      mlir::Value mask =
          constInt(rewriter, loc, type, llvm::APInt::getLowBitsSet(w, k));
      rewriter.replaceOpWithNewOp<arith::AndIOp>(op, base, mask);
      return success();
    }

    // Non-binary modulus: compute in a doubled width so the width's own
    // wraparound cannot contaminate the reduction, then truncate back.
    //
    // `2*w` is sufficient and safe: the representation width covers the
    // modulus (`modulus <= 2**w`), so the largest intermediate
    // `(m-1)**2 < 2**(2w)` fits exactly, and `m` (at most `w+1` signed
    // bits) always zero-extends *up* to the wider type, never narrows.
    unsigned ww = 2 * w;
    mlir::Type wide = mlir::IntegerType::get(rewriter.getContext(), ww);
    mlir::Value a = rewriter.create<arith::ExtUIOp>(loc, wide, lhs);
    mlir::Value b = rewriter.create<arith::ExtUIOp>(loc, wide, rhs);
    mlir::Value mc = constInt(rewriter, loc, wide, m.zext(ww));
    mlir::Value r;
    if (kind == ada::AdaBinaryOp::Mult) {
      // The product reaches `(m-1)**2`, so a full `urem` is required.
      mlir::Value t = rewriter.create<arith::MulIOp>(loc, a, b);
      r = rewriter.create<arith::RemUIOp>(loc, t, mc);
    } else {
      // Addition and subtraction land at most one modulus outside the
      // range (`a + b < 2m`; `a + m - b` in `1 .. 2m-1`), so a single
      // conditional `- m` reduces them (a division-free correction).
      // Subtraction forms `a + m - b` first to stay non-negative (a bare
      // `a - b` would underflow).
      mlir::Value s;
      if (kind == ada::AdaBinaryOp::Plus)
        s = rewriter.create<arith::AddIOp>(loc, a, b);
      else
        s = rewriter.create<arith::SubIOp>(
            loc, rewriter.create<arith::AddIOp>(loc, a, mc), b);
      mlir::Value ge =
          rewriter.create<arith::CmpIOp>(loc, arith::CmpIPredicate::uge, s, mc);
      mlir::Value sub = rewriter.create<arith::SubIOp>(loc, s, mc);
      r = rewriter.create<arith::SelectOp>(loc, ge, sub, s);
    }
    rewriter.replaceOpWithNewOp<arith::TruncIOp>(op, type, r);
    return success();
  }

  // Plain lowering for ops needing no check or reduction: the arithmetic
  // operators on integers or floats, and the bitwise Boolean operators on i1.
  void lowerPlainArithmetic(ada::BinOp op, OpAdaptor adaptor,
                            ConversionPatternRewriter &rewriter) const {
    bool isInt = mlir::isa<mlir::IntegerType>(adaptor.getLhs().getType());
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
    // Boolean logical operators are bitwise on `i1` (integer-only, no float).
    case ada::AdaBinaryOp::And:
      rewriter.replaceOpWithNewOp<arith::AndIOp>(op, adaptor.getOperands());
      break;
    case ada::AdaBinaryOp::Or:
      rewriter.replaceOpWithNewOp<arith::OrIOp>(op, adaptor.getOperands());
      break;
    case ada::AdaBinaryOp::Xor:
      rewriter.replaceOpWithNewOp<arith::XOrIOp>(op, adaptor.getOperands());
      break;
    }
  }
};

// Lowers ada.cmp to arith.cmpi/arith.cmpf, dispatching on the relational
// operator kind and on integer vs. float operand type. The result is i1,
// matching the lowered Boolean (i1) result type.
struct CmpOpLowering : public OpConversionPattern<ada::CmpOp> {
  using OpConversionPattern<ada::CmpOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::CmpOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    Type type = adaptor.getLhs().getType();
    bool isInt = mlir::isa<mlir::IntegerType>(type);
    if (!isInt && !mlir::isa<mlir::FloatType>(type))
      return rewriter.notifyMatchFailure(op, [type](Diagnostic &diag) {
        diag << "unsupported operand type: " << type;
      });

    // Ordering needs the operand's signedness (equality does not): signed
    // integers order signed, modular and enumeration types unsigned.
    bool uns = false;
    if (auto qual = mlir::dyn_cast<ada::QualType>(op.getLhs().getType()))
      uns = isUnsignedOrderQualType(op, qual);

    // Map each relational operator to its integer and float predicate.
    //
    // A Boolean '/=' is not an independent operation: @rm{6-6} defines it as
    // the complementary result of '=' ("/=" with a Boolean result cannot even
    // be declared on its own). The complementary predicates realize that
    // negation exactly: 'ne' is 'not eq', and 'une' is 'not oeq' (true when
    // either operand is NaN). 'one' would only be 'not oeq' for non-NaN
    // operands, so it is not 'not (=)' and must not be used here.
    arith::CmpIPredicate iPred;
    arith::CmpFPredicate fPred;
    switch (op.getKind()) {
    case ada::AdaRelationalOp::Eq:
      iPred = arith::CmpIPredicate::eq;
      fPred = arith::CmpFPredicate::OEQ;
      break;
    case ada::AdaRelationalOp::Neq:
      iPred = arith::CmpIPredicate::ne;
      fPred = arith::CmpFPredicate::UNE;
      break;
    case ada::AdaRelationalOp::Lt:
      iPred = uns ? arith::CmpIPredicate::ult : arith::CmpIPredicate::slt;
      fPred = arith::CmpFPredicate::OLT;
      break;
    case ada::AdaRelationalOp::Lte:
      iPred = uns ? arith::CmpIPredicate::ule : arith::CmpIPredicate::sle;
      fPred = arith::CmpFPredicate::OLE;
      break;
    case ada::AdaRelationalOp::Gt:
      iPred = uns ? arith::CmpIPredicate::ugt : arith::CmpIPredicate::sgt;
      fPred = arith::CmpFPredicate::OGT;
      break;
    case ada::AdaRelationalOp::Gte:
      iPred = uns ? arith::CmpIPredicate::uge : arith::CmpIPredicate::sge;
      fPred = arith::CmpFPredicate::OGE;
      break;
    }

    if (isInt)
      rewriter.replaceOpWithNewOp<arith::CmpIOp>(op, iPred, adaptor.getLhs(),
                                                 adaptor.getRhs());
    else
      rewriter.replaceOpWithNewOp<arith::CmpFOp>(op, fPred, adaptor.getLhs(),
                                                 adaptor.getRhs());
    return success();
  }
};

// Lowers ada.unop to its arith equivalent. `not` is one's complement: a modular
// type complements within the modulus, Boolean flips its single bit.
struct UnOpLowering : public OpConversionPattern<ada::UnOp> {
  using OpConversionPattern<ada::UnOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::UnOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    switch (op.getKind()) {
    case ada::AdaUnaryOp::Not: {
      mlir::Location loc = op.getLoc();
      mlir::Type type = adaptor.getOperand().getType();
      mlir::IntegerAttr modulus;
      if (auto qual = mlir::dyn_cast<ada::QualType>(op.getOperand().getType()))
        modulus = getModularModulus(op, qual);
      // A modular `not` complements within the modulus: `(modulus - 1) - X`
      // (correct for any modulus; a full-width `2**w` one folds to the all-ones
      // xor). Boolean is a single-bit flip: xor with true.
      if (modulus) {
        unsigned w = mlir::cast<mlir::IntegerType>(type).getWidth();
        mlir::Value hi = constInt(rewriter, loc, type,
                                  modulus.getValue().zextOrTrunc(w) - 1);
        rewriter.replaceOpWithNewOp<arith::SubIOp>(op, hi,
                                                   adaptor.getOperand());
      } else {
        mlir::Value t = constInt(rewriter, loc, type, 1);
        rewriter.replaceOpWithNewOp<arith::XOrIOp>(op, adaptor.getOperand(), t);
      }
      return success();
    }
    }
    return rewriter.notifyMatchFailure(op, "unsupported unary operator");
  }
};

// ada.unwrap exposes the builtin value under an ada.qual annotation. The type
// converter maps ada.qual<T> to T, so the adaptor's operand is already the
// target type; the op is a no-op and is replaced by its operand.
struct UnwrapOpLowering : public OpConversionPattern<ada::UnwrapOp> {
  using OpConversionPattern<ada::UnwrapOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(ada::UnwrapOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    rewriter.replaceOp(op, adaptor.getValue());
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

    // Collect Ada type refs for input params before region move. A
    // by-reference (out/in out) parameter is a memref of its ada.qual; an in
    // parameter is the ada.qual directly. Record the Ada subtype either way so
    // AdaDebugInfoPass describes the parameter with its own type rather than
    // re-inferring the base type from a load/store.
    SmallVector<mlir::FlatSymbolRefAttr> paramTypeRefs;
    for (mlir::Type t : funcType.getInputs()) {
      if (auto mr = dyn_cast<MemRefType>(t))
        t = mr.getElementType();
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
  // !ada.range<T, @s> is a bound descriptor: a struct of two machine-typed
  // bounds. The subtype symbol is representation-only and drops here.
  typeConverter.addConversion([&](ada::RangeType t) -> mlir::Type {
    mlir::Type bound = typeConverter.convertType(t.getBoundType());
    if (!bound)
      return {};
    return LLVM::LLVMStructType::getLiteral(t.getContext(), {bound, bound});
  });

  // Provide the patterns used for lowering.
  RewritePatternSet patterns(&getContext());
  // Lower structured control flow (scf.if from if expressions, @rm{4-5-7}) to
  // unstructured cf, which the cf patterns below then take to LLVM.
  mlir::populateSCFToControlFlowConversionPatterns(patterns);
  mlir::arith::populateArithToLLVMConversionPatterns(typeConverter, patterns);
  cf::populateControlFlowToLLVMConversionPatterns(typeConverter, patterns);
  populateFuncToLLVMConversionPatterns(typeConverter, patterns);
  populateFinalizeMemRefToLLVMConversionPatterns(typeConverter, patterns);

  patterns.add<NullOpLowering>(&getContext());
  patterns.add<ReturnOpLowering, CallOpLowering, BinOpLowering, CmpOpLowering,
               UnOpLowering, UnwrapOpLowering, SubpOpLowering,
               ConstantOpLowering, CoerceOpLowering, RangeOpLowering,
               RangeCheckOpLowering, AttrOpLowering>(typeConverter,
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
