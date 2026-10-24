// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2024-2026 The LALVM Project

#ifndef ADA_DIALECT_H
#define ADA_DIALECT_H

#include "mlir/Bytecode/BytecodeOpInterface.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Interfaces/CallInterfaces.h"
#include "mlir/Interfaces/ControlFlowInterfaces.h"
#include "mlir/Interfaces/FunctionInterfaces.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "mlir/Interfaces/MemorySlotInterfaces.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/APSInt.h"

/// Include the auto-generated header file containing the declaration of the Ada
/// dialect.
#include "ada/Dialect.h.inc"

/// Include the auto-generated enum declarations (AdaBinaryOp, AdaBinaryOpAttr).
#include "ada/AdaOpsEnums.h.inc"

/// Include the auto-generated attribute declarations (EnumTypeInfoAttr, ...).
#define GET_ATTRDEF_CLASSES
#include "ada/Attrs.h.inc"

/// Include the auto-generated type declarations (QualType, ...).
#define GET_TYPEDEF_CLASSES
#include "ada/Types.h.inc"

/// Include the auto-generated header file containing the declarations of the
/// Ada operations.
#define GET_OP_CLASSES
#include "ada/Ops.h.inc"

namespace mlir {
namespace ada {
/// Return the bare Ada name from a qualified dialect symbol: the segment after
/// the last dot, with a trailing __N collision suffix removed. Used for
/// human-facing names (DWARF DW_AT_name) where the simple source name is wanted
/// rather than the qualified, collision-disambiguated symbol.
llvm::StringRef bareName(llvm::StringRef qualified);

/// Bare name prefix of the anonymous subtypes MLIRGen generates.
inline constexpr llvm::StringLiteral kAnonymousTypePrefix = "__anonymous_";

/// Return the GNAT ABI symbol of a library-level subprogram: `name` with the
/// `_ada_` prefix.
std::string libraryLevelSymbol(llvm::StringRef name);

/// Resolve the nested symbol `name` visible from `from`, honoring Ada scoping:
/// walk the enclosing scopes and, at each `ada.subp`, search its `ada.decls`
/// SymbolTable children (which `lookupNearestSymbolFrom` cannot reach, since
/// `ada.subp` is not a SymbolTable and `ada.decls` is a sibling of statements).
/// Returns the symbol op, or null.
mlir::Operation *lookupSymbolFrom(mlir::Operation *from, llvm::StringRef name);

/// The `ada.type` named by `sym`, looked up from `from` (or anywhere in the
/// module, for a subprogram hoisted out of its scope), or null.
TypeOp lookupAdaType(mlir::Operation *from, mlir::FlatSymbolRefAttr sym);

/// The base type of the Ada type `sym`, or `sym` itself when it is a base
/// type. Null when `sym` does not resolve to an `ada.type`.
mlir::FlatSymbolRefAttr baseOfAdaType(mlir::Operation *from,
                                      mlir::FlatSymbolRefAttr sym);

/// The root of an Ada type, following `base` links, or null.
TypeOp rootOfAdaType(TypeOp typeOp);
TypeOp rootOfAdaType(mlir::Operation *from, mlir::FlatSymbolRefAttr sym);

/// The modulus of an Ada type if it is modular (@rm{3-5-4}), or null.
mlir::IntegerAttr modulusOfAdaType(TypeOp typeOp);
mlir::IntegerAttr modulusOfAdaType(mlir::Operation *from,
                                   mlir::FlatSymbolRefAttr sym);

/// The integer metadata of the Ada type `sym`, or of its base for a pure
/// renaming, or null.
IntegerTypeInfoAttr intTypeInfoOfAdaType(mlir::Operation *from,
                                         mlir::FlatSymbolRefAttr sym);

/// Whether `value` lies in `lo..hi`. The three values may have different bit
/// widths and signedness.
bool inStaticRange(const llvm::APSInt &value, const llvm::APSInt &lo,
                   const llvm::APSInt &hi);

/// The value of a static bound attribute (`int_info`, `array_info`), stored
/// signless at its minimal signed width, so read as signed.
llvm::APSInt boundValue(mlir::IntegerAttr bound);

/// Build an integer `ada.constant` of `type`, `value` fit to its width.
mlir::Value createIntConstant(mlir::OpBuilder &builder, mlir::Location loc,
                              QualType type, const llvm::APInt &value);

/// Build an `ada.range` descriptor of subtype `sym` from base type bounds.
mlir::Value createRange(mlir::OpBuilder &builder, mlir::Location loc,
                        mlir::Value lo, mlir::Value hi,
                        mlir::FlatSymbolRefAttr sym);

/// A constant-bounds `ada.range` descriptor of subtype `sym`: one earlier in
/// the current block with the same bounds, or a new one with `baseQual` bounds.
mlir::Value getOrCreateStaticRange(mlir::OpBuilder &builder, mlir::Location loc,
                                   mlir::FlatSymbolRefAttr sym,
                                   QualType baseQual, const llvm::APInt &lo,
                                   const llvm::APInt &hi);

/// `loc` without the object name MLIRGen may attach to a value.
mlir::Location unnamedLoc(mlir::Location loc);

/// If `loc` is a NameLoc, wrap its child in a FusedLoc that carries the Ada
/// type reference `adaType` as metadata.
mlir::Location attachAdaTypeRef(mlir::Location loc,
                                mlir::FlatSymbolRefAttr adaType);
} // namespace ada
} // namespace mlir

#endif // ADA_DIALECT_H
