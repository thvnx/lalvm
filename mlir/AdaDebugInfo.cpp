//===- AdaDebugInfo.cpp - Emit LLVM debug intrinsics from NameLoc ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass emits LLVM debug intrinsics for Ada objects and parameters.
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
// Ada types are resolved by looking up the op's MLIR type against surviving
// `ada.type` ops (keyed by getMlirType()). For llvm.alloca the type is
// getElemType(); for scalar ops it is the result type; for scalar block args
// it is the arg type; for ptr block args it is inferred from the first
// llvm.load or llvm.store use of the argument.
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
// ada.type ops survive LowerToLLVM (marked legal) to provide Ada type names
// for debug info. They are dropped by AdaToLLVMIRTranslation after this pass.
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

/// Build a DIBasicTypeAttr from an MLIR type with an explicit name.
/// Returns null for unsupported MLIR types (not IntegerType or FloatType).
static LLVM::DIBasicTypeAttr makeDIBasicType(MLIRContext *ctx, StringRef name,
                                             mlir::Type mlirType) {
  if (auto intType = dyn_cast<IntegerType>(mlirType)) {
    unsigned w = intType.getWidth();
    unsigned enc =
        (w == 1) ? llvm::dwarf::DW_ATE_boolean : llvm::dwarf::DW_ATE_signed;
    return LLVM::DIBasicTypeAttr::get(ctx, llvm::dwarf::DW_TAG_base_type, name,
                                      llvm::alignTo(w, 8), enc);
  }
  if (auto floatType = dyn_cast<FloatType>(mlirType))
    return LLVM::DIBasicTypeAttr::get(ctx, llvm::dwarf::DW_TAG_base_type, name,
                                      floatType.getWidth(),
                                      llvm::dwarf::DW_ATE_float);
  return {};
}

/// Build a DIBasicTypeAttr for an integer type with a generic "integer_N" name.
static LLVM::DIBasicTypeAttr makeDIIntType(MLIRContext *ctx,
                                           IntegerType intType) {
  return makeDIBasicType(
      ctx, ("integer_" + llvm::Twine(intType.getWidth())).str(), intType);
}

/// Build a DIBasicTypeAttr using the Ada type name from `typeOp`.
/// Returns null if the MLIR type is neither IntegerType nor FloatType.
static LLVM::DIBasicTypeAttr makeDINamedType(MLIRContext *ctx,
                                             ada::TypeOp typeOp) {
  // Modular types use DW_ATE_unsigned instead of DW_ATE_signed.
  if (auto numInfo = dyn_cast<ada::NumericTypeInfoAttr>(typeOp.getTypeInfo()))
    if (numInfo.getModulus())
      return LLVM::DIBasicTypeAttr::get(
          ctx, llvm::dwarf::DW_TAG_base_type,
          ada::bareName(typeOp.getSymName()),
          llvm::alignTo(cast<IntegerType>(typeOp.getMlirType()).getWidth(), 8),
          llvm::dwarf::DW_ATE_unsigned);
  return makeDIBasicType(ctx, ada::bareName(typeOp.getSymName()),
                         typeOp.getMlirType());
}

/// Build an empty DICompositeTypeAttr stub for an enum ada.type op.
/// The empty elements array acts as a sentinel: buildEnumDITypes replaces it
/// with the full DICompositeType after MLIR-to-LLVM translation.
static LLVM::DICompositeTypeAttr makeDIEnumStub(MLIRContext *ctx,
                                                ada::TypeOp typeOp) {
  auto intType = dyn_cast<IntegerType>(typeOp.getMlirType());
  if (!intType)
    return {};
  return LLVM::DICompositeTypeAttr::get(
      ctx, llvm::dwarf::DW_TAG_enumeration_type,
      // Full sym_name: this stub's name is the key matched against the
      // post-translation enum types in buildEnumDITypes (EnumDITypes.cpp).
      StringAttr::get(ctx, typeOp.getSymName()),
      /*file=*/LLVM::DIFileAttr{}, /*line=*/0, /*scope=*/LLVM::DIScopeAttr{},
      /*baseType=*/LLVM::DITypeAttr{}, LLVM::DIFlags::Zero,
      llvm::alignTo(intType.getWidth(), 8), /*alignInBits=*/0,
      /*elements=*/{}, /*dataLocation=*/LLVM::DIExpressionAttr{},
      /*rank=*/LLVM::DIExpressionAttr{},
      /*allocated=*/LLVM::DIExpressionAttr{},
      /*associated=*/LLVM::DIExpressionAttr{});
}

/// Dispatch to the appropriate DI type for a given ada.type op.
/// Returns a DICompositeTypeAttr stub for enum types, a DIBasicTypeAttr for
/// numeric types, and null for unsupported type info kinds.
static LLVM::DITypeAttr makeDITypeAttr(MLIRContext *ctx, ada::TypeOp typeOp) {
  if (isa<ada::EnumTypeInfoAttr>(typeOp.getTypeInfo()))
    return makeDIEnumStub(ctx, typeOp);
  if (!isa<ada::NumericTypeInfoAttr>(typeOp.getTypeInfo()))
    return {};
  return makeDINamedType(ctx, typeOp);
}

/// Look up a named DI type for `mlirType` in `typeOpCache`.
/// Returns null if mlirType has no matching ada.type op or no recognized type
/// info. Returns a DICompositeTypeAttr stub for enum types (replaced later by
/// buildEnumDITypes) and a DIBasicTypeAttr for numeric types.
static LLVM::DITypeAttr
lookupNamedDIType(MLIRContext *ctx, mlir::Type mlirType,
                  llvm::DenseMap<mlir::Type, ada::TypeOp> &typeOpCache) {
  auto it = typeOpCache.find(mlirType);
  if (it == typeOpCache.end())
    return {};
  return makeDITypeAttr(ctx, it->second);
}

/// Extract the Ada-type-specific DI type from a FusedLoc carrying a
/// FlatSymbolRefAttr, using `typeOpByName` to resolve the sym_name.
/// Returns null if the loc has no such metadata or the sym_name is not found.
static LLVM::DITypeAttr extractDITypeFromLoc(
    MLIRContext *ctx, Location loc,
    llvm::DenseMap<mlir::StringAttr, ada::TypeOp> &typeOpByName) {
  auto fl = dyn_cast<FusedLoc>(loc);
  if (!fl)
    return {};
  auto typeRef = dyn_cast_or_null<ada::DITypeRefAttr>(fl.getMetadata());
  if (!typeRef)
    return {};
  auto it = typeOpByName.find(typeRef.getSym().getAttr());
  if (it == typeOpByName.end())
    return {};
  return makeDITypeAttr(ctx, it->second);
}

static std::pair<LLVM::DIFileAttr, unsigned>
getFileAndLine(MLIRContext *ctx, Location loc,
               LLVM::DISubprogramAttr subprogram) {
  // Unwrap FusedLoc carrying Ada type metadata (from attachAdaTypeRef).
  if (auto fl = dyn_cast<FusedLoc>(loc))
    if (!fl.getLocations().empty())
      return getFileAndLine(ctx, fl.getLocations()[0], subprogram);
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

struct AdaDebugInfoPass
    : public PassWrapper<AdaDebugInfoPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(AdaDebugInfoPass)

  void runOnOperation() final {
    MLIRContext *ctx = &getContext();
    ModuleOp module = getOperation();

    // Pass 0: nested subprograms are private (internal linkage); derived from
    // the visibility carried onto the llvm.func, mark their DISubprogram local
    // to the unit so DWARF does not emit DW_AT_external for them. Run first so
    // the later passes read the updated subprogram.
    module.walk([&](LLVM::LLVMFuncOp func) {
      if (!func.isPrivate())
        return;
      auto fused = dyn_cast<FusedLoc>(func.getLoc());
      if (!fused)
        return;
      auto sp = dyn_cast_or_null<LLVM::DISubprogramAttr>(fused.getMetadata());
      if (!sp)
        return;
      auto newSp = LLVM::DISubprogramAttr::get(
          ctx, sp.getId(), sp.getCompileUnit(), sp.getScope(), sp.getName(),
          sp.getLinkageName(), sp.getFile(), sp.getLine(), sp.getScopeLine(),
          sp.getSubprogramFlags() | LLVM::DISubprogramFlags::LocalToUnit,
          sp.getType(), sp.getRetainedNodes(), sp.getAnnotations());
      func->setLoc(FusedLoc::get(ctx, fused.getLocations(), newSp));
    });

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

    // Build ada.type op caches for Passes 3-4.
    // typeOpCache (keyed by MLIR type) is a fallback when no Ada type ref is
    // encoded in the value's location. typeOpByName (keyed by sym_name) is the
    // primary lookup when attachAdaTypeRef embedded a FlatSymbolRefAttr.
    llvm::DenseMap<mlir::Type, ada::TypeOp> typeOpCache;
    llvm::DenseMap<mlir::StringAttr, ada::TypeOp> typeOpByName;
    module.walk([&](ada::TypeOp typeOp) {
      typeOpCache.try_emplace(typeOp.getMlirType(), typeOp);
      typeOpByName.try_emplace(typeOp.getSymNameAttr(), typeOp);
    });

    // Pass 3: emit debug intrinsics.
    for (auto &entry : entries) {
      auto [fileAttr, line] =
          getFileAndLine(ctx, entry.innerLoc, entry.subprogram);

      mlir::Type elemType = entry.isDeclare
                                ? cast<LLVM::AllocaOp>(entry.op).getElemType()
                                : entry.op->getResult(0).getType();

      LLVM::DITypeAttr diType =
          extractDITypeFromLoc(ctx, entry.innerLoc, typeOpByName);
      if (!diType)
        diType = lookupNamedDIType(ctx, elemType, typeOpCache);
      if (!diType) {
        auto intType = dyn_cast<IntegerType>(elemType);
        if (!intType)
          continue; // silently skip floats without type info
        diType = makeDIIntType(ctx, intType);
      }
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
        if (isa<IntegerType, FloatType>(argType)) {
          // Scalar `in` parameter: dbg.value at function entry.
          diType = extractDITypeFromLoc(ctx, nl.getChildLoc(), typeOpByName);
          if (!diType)
            diType = lookupNamedDIType(ctx, argType, typeOpCache);
          if (!diType) {
            auto intType = dyn_cast<IntegerType>(argType);
            if (!intType)
              continue; // float without type info: skip silently
            diType = makeDIIntType(ctx, intType);
          }
        } else if (isa<LLVM::LLVMPointerType>(argType)) {
          // Reference `in out`/`out` parameter: dbg.declare.
          // The ptr is opaque; recover the element type from the first
          // llvm.load or llvm.store that uses this argument.
          isDeclare = true;
          mlir::Type elemType;
          for (Operation *userOp : arg.getUsers()) {
            if (auto load = dyn_cast<LLVM::LoadOp>(userOp)) {
              elemType = load->getResult(0).getType();
              break;
            }
            if (auto store = dyn_cast<LLVM::StoreOp>(userOp)) {
              elemType = store.getValue().getType();
              break;
            }
          }
          if (!elemType) {
            func.emitWarning("reference parameter '")
                << nl.getName().getValue()
                << "' has no load/store uses; skipping debug info";
            continue;
          }
          diType = extractDITypeFromLoc(ctx, nl.getChildLoc(), typeOpByName);
          if (!diType)
            diType = lookupNamedDIType(ctx, elemType, typeOpCache);
          if (!diType) {
            auto intElemType = dyn_cast<IntegerType>(elemType);
            if (!intElemType)
              continue; // silently skip floats and other unsupported types
            diType = makeDIIntType(ctx, intElemType);
          }
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
};

struct DICompileUnitAdaPass
    : public PassWrapper<DICompileUnitAdaPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(DICompileUnitAdaPass)

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<LLVM::LLVMDialect>();
  }

  void runOnOperation() final {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();

    StringRef filePath;
    if (auto loc = dyn_cast<FileLineColRange>(module.getLoc()))
      filePath = loc.getFilename().getValue();

    auto fileAttr =
        LLVM::DIFileAttr::get(ctx, llvm::sys::path::filename(filePath),
                              llvm::sys::path::parent_path(filePath));
    auto cuAttr = LLVM::DICompileUnitAttr::get(
        DistinctAttr::create(UnitAttr::get(ctx)), llvm::dwarf::DW_LANG_Ada2012,
        fileAttr, StringAttr::get(ctx, "lalvm"),
        /*isOptimized=*/false, LLVM::DIEmissionKind::Full);
    module->setLoc(FusedLoc::get(ctx, {module.getLoc()}, cuAttr));
  }
};
} // namespace

std::unique_ptr<mlir::Pass> mlir::ada::createAdaDebugInfoPass() {
  return std::make_unique<AdaDebugInfoPass>();
}

std::unique_ptr<mlir::Pass> mlir::ada::createDICompileUnitAdaPass() {
  return std::make_unique<DICompileUnitAdaPass>();
}
