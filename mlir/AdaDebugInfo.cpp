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
  if (auto intInfo =
          dyn_cast_or_null<ada::IntegerTypeInfoAttr>(typeOp.getTypeInfoAttr()))
    if (intInfo.getModulus())
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
      // post-translation enum types in buildEnumDITypes
      // (PostTranslationDITypes.cpp).
      StringAttr::get(ctx, typeOp.getSymName()),
      /*file=*/LLVM::DIFileAttr{}, /*line=*/0, /*scope=*/LLVM::DIScopeAttr{},
      /*baseType=*/LLVM::DITypeAttr{}, LLVM::DIFlags::Zero,
      llvm::alignTo(intType.getWidth(), 8), /*alignInBits=*/0,
      /*dataLocation=*/LLVM::DIExpressionAttr{},
      /*rank=*/LLVM::DIExpressionAttr{},
      /*allocated=*/LLVM::DIExpressionAttr{},
      /*associated=*/LLVM::DIExpressionAttr{}, /*elements=*/{});
}

/// Build a DIDerivedType typedef placeholder for a constrained integer subtype.
/// MLIR has no DISubrangeType attr (the only LLVM node with
/// DW_TAG_subrange_type as a type), so a typedef of the base type, keyed by the
/// full sym_name, stands in until buildSubrangeDITypes replaces it with a real
/// DISubrangeType after MLIR-to-LLVM translation (matching by name). The
/// typedef is itself a valid, honest fallback should that step not run.
static LLVM::DIDerivedTypeAttr makeDISubrangeStub(MLIRContext *ctx,
                                                  ada::TypeOp typeOp,
                                                  LLVM::DITypeAttr baseType) {
  return LLVM::DIDerivedTypeAttr::get(
      ctx, llvm::dwarf::DW_TAG_typedef,
      StringAttr::get(ctx, typeOp.getSymName()), baseType, /*sizeInBits=*/0,
      /*alignInBits=*/0, /*offsetInBits=*/0, /*dwarfAddressSpace=*/std::nullopt,
      /*extraData=*/LLVM::DINodeAttr{});
}

/// Wrap `baseType` in a DW_TAG_const_type, modeling an Ada `in` parameter as a
/// read-only (constant) view (@rm{6-1}), as GNAT does.
static LLVM::DIDerivedTypeAttr makeDIConstType(MLIRContext *ctx,
                                               LLVM::DITypeAttr baseType) {
  return LLVM::DIDerivedTypeAttr::get(
      ctx, llvm::dwarf::DW_TAG_const_type, /*name=*/StringAttr{}, baseType,
      /*sizeInBits=*/0, /*alignInBits=*/0, /*offsetInBits=*/0,
      /*dwarfAddressSpace=*/std::nullopt, /*extraData=*/LLVM::DINodeAttr{});
}

/// Dispatch to the appropriate DI type for a given ada.type op.
/// Returns a DICompositeTypeAttr stub for enum types, a DIDerivedType typedef
/// stub for constrained integer subtypes, a DIBasicTypeAttr for base integer
/// and float types, and null for unsupported type info kinds.
static LLVM::DITypeAttr makeDITypeAttr(MLIRContext *ctx, ada::TypeOp typeOp) {
  auto enumInfo =
      dyn_cast_or_null<ada::EnumTypeInfoAttr>(typeOp.getTypeInfoAttr());
  // Subtypes that carry no range of their own (no literals/representation)
  // are described as their base type. Base links are acyclic by construction.
  if (!typeOp.getTypeInfoAttr() || (enumInfo && enumInfo.getNames().empty())) {
    auto baseAttr = typeOp.getBaseAttr();
    if (!baseAttr)
      return {};
    auto baseOp = dyn_cast_or_null<ada::TypeOp>(
        SymbolTable::lookupNearestSymbolFrom(typeOp, baseAttr));
    return baseOp ? makeDITypeAttr(ctx, baseOp) : LLVM::DITypeAttr();
  }
  if (enumInfo)
    return makeDIEnumStub(ctx, typeOp);
  if (!isa<ada::IntegerTypeInfoAttr, ada::FloatTypeInfoAttr>(
          typeOp.getTypeInfoAttr()))
    return {};
  // A constrained integer subtype (one carrying a range, static or dynamic) is
  // a DWARF subrange of its base type; buildSubrangeDITypes fills in the
  // bounds.
  if (auto intInfo =
          dyn_cast<ada::IntegerTypeInfoAttr>(typeOp.getTypeInfoAttr()))
    if (typeOp.getBaseAttr() && intInfo.hasRange()) {
      auto baseOp = dyn_cast_or_null<ada::TypeOp>(
          SymbolTable::lookupNearestSymbolFrom(typeOp, typeOp.getBaseAttr()));
      return makeDISubrangeStub(ctx, typeOp,
                                baseOp ? makeDITypeAttr(ctx, baseOp)
                                       : LLVM::DITypeAttr());
    }
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

/// Returns the DISubprogramAttr attached to `op` by DIScopeForLLVMFuncOpPass,
/// or a null attr if the location is not a FusedLoc with DISubprogramAttr
/// metadata (i.e. the function has no debug info).
static LLVM::DISubprogramAttr getSubprogram(Operation *op) {
  auto fl = dyn_cast<FusedLoc>(op->getLoc());
  if (!fl)
    return {};
  return dyn_cast_or_null<LLVM::DISubprogramAttr>(fl.getMetadata());
}

/// Returns `op`'s enclosing llvm.func and its DISubprogram. The subprogram is
/// null when either lookup fails, so callers test it alone.
static std::pair<LLVM::LLVMFuncOp, LLVM::DISubprogramAttr>
enclosingFuncAndSubprogram(Operation *op) {
  auto func = op->getParentOfType<LLVM::LLVMFuncOp>();
  if (!func)
    return {};
  return {func, getSubprogram(func)};
}

/// Collect every DILabelRef marker in a location's FusedLoc tree. A location
/// can carry more than one when consecutive labels resolve to the same anchor
/// op.
static void collectLabelRefs(Location loc,
                             SmallVectorImpl<ada::DILabelRefAttr> &out) {
  if (auto fused = dyn_cast<FusedLoc>(loc)) {
    if (auto m = dyn_cast<ada::DILabelRefAttr>(fused.getMetadata()))
      out.push_back(m);
    for (Location inner : fused.getLocations())
      collectLabelRefs(inner, out);
  }
}

/// Rebuild `sp` overriding its name, flags, and subroutine type, preserving
/// every other field (notably linkageName). DISubprogramAttr has no copy-with,
/// so the full get() is unavoidable; this keeps the boilerplate in one place.
static LLVM::DISubprogramAttr cloneSubprogram(LLVM::DISubprogramAttr sp,
                                              StringAttr name,
                                              LLVM::DISubprogramFlags flags,
                                              LLVM::DISubroutineTypeAttr type) {
  return LLVM::DISubprogramAttr::get(
      sp.getContext(), sp.getId(), sp.getCompileUnit(), sp.getScope(), name,
      sp.getLinkageName(), sp.getFile(), sp.getLine(), sp.getScopeLine(), flags,
      type, sp.getRetainedNodes(), sp.getAnnotations());
}

struct AdaDebugInfoPass
    : public PassWrapper<AdaDebugInfoPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(AdaDebugInfoPass)

  void runOnOperation() final {
    MLIRContext *ctx = &getContext();
    ModuleOp module = getOperation();

    // Pass 0: fix up the DISubprogram fields DIScopeForLLVMFuncOpPass could not
    // set correctly from the mangled llvm.func name alone. Run first so the
    // later passes read the updated subprogram.
    //   - DW_AT_name: use the bare Ada source name (carried as a DINameAttr on
    //     the function location by HoistNestedSymbolOperations); DW_AT_linkage
    //     stays the mangled symbol.
    //   - DW_AT_external: private (nested) subprograms have internal linkage,
    //     so their DISubprogram is marked local to the unit.
    module.walk([&](LLVM::LLVMFuncOp func) {
      auto fused = dyn_cast<FusedLoc>(func.getLoc());
      if (!fused)
        return;
      auto sp = dyn_cast_or_null<LLVM::DISubprogramAttr>(fused.getMetadata());
      if (!sp)
        return;

      StringAttr name = sp.getName();
      if (auto nameLoc =
              func.getLoc()->findInstanceOf<FusedLocWith<ada::DINameAttr>>())
        name = nameLoc.getMetadata().getName();

      LLVM::DISubprogramFlags flags = sp.getSubprogramFlags();
      if (func.isPrivate())
        flags = flags | LLVM::DISubprogramFlags::LocalToUnit;

      if (name == sp.getName() && flags == sp.getSubprogramFlags())
        return; // nothing to change

      auto newSp = cloneSubprogram(sp, name, flags, sp.getType());
      func->setLoc(FusedLoc::get(ctx, fused.getLocations(), newSp));
    });

    // Build ada.type op caches used by the remaining passes.
    // typeOpCache (keyed by MLIR type) is a fallback when no Ada type ref is
    // encoded in the value's location. typeOpByName (keyed by sym_name) is the
    // primary lookup when attachAdaTypeRef embedded a FlatSymbolRefAttr.
    llvm::DenseMap<mlir::Type, ada::TypeOp> typeOpCache;
    llvm::DenseMap<mlir::StringAttr, ada::TypeOp> typeOpByName;
    module.walk([&](ada::TypeOp typeOp) {
      typeOpCache.try_emplace(typeOp.getMlirType(), typeOp);
      typeOpByName.try_emplace(typeOp.getSymNameAttr(), typeOp);
    });

    // Pass 1: populate each subprogram's DISubroutineType so DWARF emits the
    // return type as the subprogram's DW_AT_type and records the full
    // signature. Element 0 is the return type (null = void/procedure), carried
    // by LowerToLLVM as a DITypeRef nested one level under the DISubprogram's
    // FusedLoc; the remaining elements are the parameter types, read from the
    // entry block-arg locations (the same metadata Pass 6 uses). Runs before
    // the variable/parameter passes so their DIEs reference the rebuilt
    // subprogram as scope.
    module.walk([&](LLVM::LLVMFuncOp func) {
      // Definitions only: parameter types come from the entry block args, so a
      // bodyless declaration would yield a misleading param-less signature.
      if (func.getBody().empty())
        return;
      auto fused = dyn_cast<FusedLoc>(func.getLoc());
      if (!fused)
        return;
      auto sp = dyn_cast_or_null<LLVM::DISubprogramAttr>(fused.getMetadata());
      if (!sp)
        return;

      // Element 0: return type, from the DITypeRef nested under the function's
      // FusedLoc. Null for procedures (no nested DITypeRef) = void return.
      LLVM::DITypeAttr returnType;
      if (!fused.getLocations().empty())
        returnType =
            extractDITypeFromLoc(ctx, fused.getLocations()[0], typeOpByName);

      // Remaining elements: parameter types from entry block-arg locations.
      SmallVector<LLVM::DITypeAttr> types{returnType};
      for (BlockArgument arg : func.getBody().front().getArguments()) {
        auto nl = dyn_cast<NameLoc>(arg.getLoc());
        types.push_back(
            nl ? extractDITypeFromLoc(ctx, nl.getChildLoc(), typeOpByName)
               : LLVM::DITypeAttr{});
      }

      auto subType = LLVM::DISubroutineTypeAttr::get(
          ctx, llvm::dwarf::DW_CC_normal, types);
      auto newSp =
          cloneSubprogram(sp, sp.getName(), sp.getSubprogramFlags(), subType);
      func->setLoc(FusedLoc::get(ctx, fused.getLocations(), newSp));
    });

    // Pass 2: emit a DW_TAG_label for each Ada goto label (@rm{5-8}). Walk the
    // DILabelRef markers MLIRGen fused onto the label anchor ops, collecting a
    // DILabel per marker (deduped on (func, name)), then emit an
    // llvm.intr.dbg.label at each anchor (its address is the label's low_pc).
    // @todo LLVM 21's DILabel has no column field; take it from the loc once
    //       DILabel gains one.
    llvm::SmallVector<std::pair<Operation *, LLVM::DILabelAttr>> labelSites;
    llvm::DenseSet<std::pair<Operation *, StringAttr>> emittedLabels;
    module.walk([&](Operation *op) {
      SmallVector<ada::DILabelRefAttr> refs;
      collectLabelRefs(op->getLoc(), refs);
      if (refs.empty())
        return;
      auto [func, sp] = enclosingFuncAndSubprogram(op);
      if (!sp)
        return;
      for (ada::DILabelRefAttr ref : refs) {
        StringAttr name = ref.getName();
        if (!emittedLabels.insert({func.getOperation(), name}).second)
          continue;
        auto [fileAttr, line] = getFileAndLine(ctx, ref.getLoc(), sp);
        labelSites.push_back(
            {op, LLVM::DILabelAttr::get(ctx, sp, name, fileAttr, line)});
      }
    });
    for (auto &[op, label] : labelSites) {
      // The dbg.label needs its own scoped !dbg (a DILocation under the
      // subprogram), or MLIR-to-LLVM translation drops it; the anchor op's loc
      // may be unscoped (e.g. an implicit return with an unknown location).
      Location scoped = FusedLoc::get(
          ctx,
          {FileLineColLoc::get(label.getFile().getName(), label.getLine(),
                               /*column=*/0)},
          label.getScope());
      OpBuilder(op).create<LLVM::DbgLabelOp>(scoped, label);
    }

    // Pass 3: record (func, name) pairs for all llvm.alloca ops with NameLoc.
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

    // Pass 4: collect debug entries.
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

      auto [func, subprogram] = enclosingFuncAndSubprogram(op);
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

    // Pass 5: emit debug intrinsics.
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

    // Pass 6: emit DW_TAG_formal_parameter intrinsics for llvm.func entry
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
          // Ada `in` parameters are read-only (@rm{6-1}): wrap in const, as
          // GNAT does (reference `in out`/`out` params below stay non-const).
          diType = makeDIConstType(ctx, diType);
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

    // Pass 7: pick the variable carrying each dynamic subrange bound.
    // LowerToLLVM tagged each non-constant descriptor bound (an `insertvalue`)
    // with its subtype symbol (a DIDynBoundAttr on the value's location); the
    // insertvalue position (0 low, 1 high) names the bound. When an existing
    // variable already carries the bound value (described by a dbg.value),
    // record it (as GNAT does); otherwise create an artificial
    // "<subtype>'first"/"'last" variable. The chosen names are accumulated per
    // subtype and attached as DISubrangeBounds metadata below, where
    // buildSubrangeDITypes reads them.
    //
    // Limitation: in practice only a subprogram *parameter* bound is reused,
    // because a parameter survives to this pass as an entry block-arg with a
    // dbg.value. A bound that is a local object does not: mem2reg promotes its
    // alloca away before this pass, dropping the NameLoc that AdaDebugInfoPass
    // would emit debug info from, so no dbg.value names it (and an unpromoted
    // local would carry a dbg.declare, which this scan does not match). Such a
    // bound therefore falls back to the artificial variable, exactly like a
    // computed bound. Reusing a local would require preserving its debug
    // identity through mem2reg, or tracing the bound load back to the alloca's
    // dbg.declare.
    //
    // A deeper limitation is cross-scope bounds. GNAT emits one shared subrange
    // that references the source object's variable directly and relies on
    // DW_AT_static_link on each nested subprogram so the debugger can walk up
    // to the enclosing frame where that variable lives. lalvm cannot mirror
    // this: ClosureConversion has already rewritten up-level references into
    // explicit captured parameters, and HoistNestedSymbolOperations flattens
    // every nested subprogram to module level, so there is no lexical nesting
    // and no static link/frame chain to follow. A bound variable is thus only
    // meaningful in the subprogram that emits it; a subtype used from a nested
    // scope cannot reference the parent scope's bound. Fully supporting all
    // cases would require either preserving lexical nesting and emitting
    // DW_AT_static_link (the GNAT model), or building a per-scope subrange copy
    // keyed off each scope's captured bound.
    llvm::DenseMap<Operation *, std::pair<StringAttr, StringAttr>> boundVars;
    module.walk([&](LLVM::InsertValueOp ins) {
      auto dynBound =
          ins.getLoc()->findInstanceOf<FusedLocWith<ada::DIDynBoundAttr>>();
      if (!dynBound)
        return;
      FlatSymbolRefAttr sym = dynBound.getMetadata().getSubtype();
      auto [func, subprogram] = enclosingFuncAndSubprogram(ins);
      if (!subprogram)
        return;
      auto intType = dyn_cast<IntegerType>(ins.getValue().getType());
      if (!intType)
        return;
      ada::TypeOp subtypeOp = typeOpByName.lookup(sym.getAttr());
      if (!subtypeOp)
        return;
      bool isUpper = !ins.getPosition().empty() && ins.getPosition()[0] == 1;
      mlir::Value boundVal = ins.getValue();

      // Prefer the source object's own variable, as GNAT does: when an existing
      // variable described by a dbg.value from an earlier pass already carries
      // the bound value, reference it directly rather than an artificial copy.
      // In practice this matches a subprogram parameter (see the limitation
      // noted above); a computed bound, or a local whose debug identity did not
      // survive mem2reg, gets an artificial variable bound to the value, named
      // "<subtype>'first/'last".
      StringAttr boundVarName;
      func.walk([&](LLVM::DbgValueOp dv) {
        if (!boundVarName && dv.getValue() == boundVal)
          boundVarName = dv.getVarInfo().getName();
      });
      if (!boundVarName) {
        boundVarName =
            StringAttr::get(ctx, (llvm::Twine(ada::bareName(sym.getValue())) +
                                  (isUpper ? "'last" : "'first"))
                                     .str());
        // Describe the artificial bound with the subtype's named base DIType
        // (the same node the subrange's baseType uses), not a generic
        // integer_N.
        LLVM::DITypeAttr boundType;
        if (auto baseAttr = subtypeOp.getBaseAttr())
          if (auto baseOp = dyn_cast_or_null<ada::TypeOp>(
                  SymbolTable::lookupNearestSymbolFrom(subtypeOp, baseAttr)))
            boundType = makeDITypeAttr(ctx, baseOp);
        if (!boundType)
          boundType = makeDIIntType(ctx, intType);
        auto [fileAttr, line] = getFileAndLine(ctx, ins.getLoc(), subprogram);
        auto varInfo = LLVM::DILocalVariableAttr::get(
            subprogram, boundVarName, fileAttr, line, /*arg=*/0,
            /*alignInBits=*/0, boundType, LLVM::DIFlags::Artificial);
        OpBuilder b(ins);
        b.setInsertionPointAfter(ins);
        LLVM::DbgValueOp::create(b, ins.getLoc(), boundVal, varInfo);
      }
      // Record which variable carries this bound; buildSubrangeDITypes looks it
      // up by (this subprogram's scope, name). Last writer wins per subtype,
      // which is unambiguous only because ClosureConversion materializes a
      // subtype's range descriptor once (in its declaring scope) and passes it
      // to nested subprograms as a captured parameter, so each bound has
      // exactly one tagged insertvalue.
      auto &vars = boundVars[subtypeOp.getOperation()];
      (isUpper ? vars.second : vars.first) = boundVarName;
    });

    // Attach the accumulated bound-variable names to each subtype's location as
    // DISubrangeBounds metadata (a null side stays a static bound).
    for (auto &[op, vars] : boundVars)
      op->setLoc(FusedLoc::get(
          ctx, {op->getLoc()},
          ada::DISubrangeBoundsAttr::get(ctx, vars.first, vars.second)));
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
