//===- Ops.td - Ada dialect operation definitions ----------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines the operations of the Ada dialect.
//
//===----------------------------------------------------------------------===//

#ifndef ADA_OPS
#define ADA_OPS

include "Dialect.td"
include "Attrs.td"
include "Types.td"
include "mlir/IR/EnumAttr.td"
include "mlir/Interfaces/FunctionInterfaces.td"
include "mlir/IR/SymbolInterfaces.td"
include "mlir/Interfaces/MemorySlotInterfaces.td"
include "mlir/Interfaces/SideEffectInterfaces.td"
include "mlir/Interfaces/CallInterfaces.td"
include "mlir/Interfaces/ControlFlowInterfaces.td"
include "mlir/Interfaces/InferTypeOpInterface.td"

//===----------------------------------------------------------------------===//
// Ada Operations
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Operators
//===----------------------------------------------------------------------===//

// Logical operators (@rm{4-5-1})
def AdaOp_And     : I32EnumAttrCase<"And",     0,  "and">;
def AdaOp_Or      : I32EnumAttrCase<"Or",      1,  "or">;
def AdaOp_Xor     : I32EnumAttrCase<"Xor",     2,  "xor">;
def AdaOp_AndThen : I32EnumAttrCase<"AndThen", 3,  "and then">;
def AdaOp_OrElse  : I32EnumAttrCase<"OrElse",  4,  "or else">;
// Relational operators (@rm{4-5-2})
def AdaOp_Eq      : I32EnumAttrCase<"Eq",      5,  "=">;
def AdaOp_Neq     : I32EnumAttrCase<"Neq",     6,  "/=">;
def AdaOp_Lt      : I32EnumAttrCase<"Lt",      7,  "<">;
def AdaOp_Lte     : I32EnumAttrCase<"Lte",     8,  "<=">;
def AdaOp_Gt      : I32EnumAttrCase<"Gt",      9,  ">">;
def AdaOp_Gte     : I32EnumAttrCase<"Gte",     10, ">=">;
def AdaOp_In      : I32EnumAttrCase<"In",      11, "in">;
def AdaOp_NotIn   : I32EnumAttrCase<"NotIn",   12, "not in">;
// Binary and unary adding operators (@rm{4-5-3}/@rm{4-5-4})
def AdaOp_Plus    : I32EnumAttrCase<"Plus",    13, "+">;
def AdaOp_Minus   : I32EnumAttrCase<"Minus",   14, "-">;
def AdaOp_Concat  : I32EnumAttrCase<"Concat",  15, "&">;
// Multiplying operators (@rm{4-5-5})
def AdaOp_Mult    : I32EnumAttrCase<"Mult",    16, "*">;
def AdaOp_Div     : I32EnumAttrCase<"Div",     17, "/">;
def AdaOp_Mod     : I32EnumAttrCase<"Mod",     18, "mod">;
def AdaOp_Rem     : I32EnumAttrCase<"Rem",     19, "rem">;
// Highest precedence operators (@rm{4-5-6})
def AdaOp_Pow     : I32EnumAttrCase<"Pow",     20, "**">;
def AdaOp_Abs     : I32EnumAttrCase<"Abs",     21, "abs">;
def AdaOp_Not     : I32EnumAttrCase<"Not",     22, "not">;

//===----------------------------------------------------------------------===//
// AdaBinaryOp
//===----------------------------------------------------------------------===//

def AdaBinaryOp : I32EnumAttr<"AdaBinaryOp",
    "Ada predefined binary operator kind",
    [AdaOp_Plus, AdaOp_Minus, AdaOp_Mult, AdaOp_Div]> {
  let cppNamespace = "::mlir::ada";
}

//===----------------------------------------------------------------------===//
// AdaRelationalOp
//===----------------------------------------------------------------------===//

def AdaRelationalOp : I32EnumAttr<"AdaRelationalOp",
    "Ada predefined relational operator kind",
    [AdaOp_Eq, AdaOp_Neq]> {
  let cppNamespace = "::mlir::ada";
}

//===----------------------------------------------------------------------===//
// AdaChecks
//===----------------------------------------------------------------------===//

// Run-time checks an arithmetic operation must perform. `overflow` (@rm{4-5},
// @rm{3-5-4}) is set on signed-integer base-type `+`/`-`/`*`; modular
// arithmetic wraps and is never flagged. `division` (@rm{11-5}) is set on
// integer `/` for both signed and modular types. Absent (no bits) means no
// check is required.
def AdaCheck_Overflow : I32BitEnumAttrCaseBit<"Overflow", 0, "overflow">;
def AdaCheck_Division : I32BitEnumAttrCaseBit<"Division", 1, "division">;

def AdaChecks : I32BitEnumAttr<"AdaChecks", "Ada arithmetic run-time checks",
    [AdaCheck_Overflow, AdaCheck_Division]> {
  let cppNamespace = "::mlir::ada";
}

//===----------------------------------------------------------------------===//
// TypeOp
//===----------------------------------------------------------------------===//

// Ada type declaration: carries type metadata through the pipeline.
// Carries the `Symbol` trait. To keep the symbol's parent a `SymbolTable` at
// every stage, `HoistNestedSymbolOperations` lifts all `ada.type` ops to module
// level before lowering, just like nested subprograms. This keeps them out of
// `llvm.func` bodies (whose parent `LLVM::LLVMFuncOp` is not a `SymbolTable`),
// where a `Symbol` would otherwise fail the post-pass verifier.
def TypeOp : Ada_Op<"type", [Symbol]> {
  let summary = "Ada type declaration (metadata only)";
  let description = [{
    Metadata-only op with no runtime value: carries type information (name,
    MLIR type, and kind-specific data) through the pipeline for downstream
    consumers (e.g. DWARF debug info generation). Not erased during lowering;
    survives the full pipeline.

    `type_info` is one of the Ada dialect type info attributes. `base` names
    the canonical base type when the declaration is a subtype; absent when
    the type is its own base. An op records only what its own declaration
    says: a subtype that adds no constraint is a pure renaming and carries no
    `type_info` at all; consumers follow `base` for inherited metadata
    (DWARF's typedef-vs-subrange distinction depends on this).

    Syntax example:

    ```mlir
    ada.type @color : i3 = #ada.enum_info<"red" = 0, "green" = 1, "blue" = 2>
    ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
    ada.type @positive base @integer : i32 = #ada.int_info<range 1 to 2147483647>
    ada.type @int base @integer : i32
    ```
  }];
  let arguments = (ins
    SymbolNameAttr:$sym_name,                 // Ada type declaration name
    TypeAttr:$mlir_type,                      // Corresponding MLIR type
    OptionalAttr<AnyAttr>:$type_info,         // Declaration-own metadata
    OptionalAttr<FlatSymbolRefAttr>:$base     // Canonical base type, if any
  );

  let skipDefaultBuilders = 1;
  let builders = [
    // `CArg` gives `base` a C++ default argument (null = no base type) in
    // the generated declaration, keeping the many base-less call sites
    // unchanged within the single builder Dialect.cpp's doxygen note allows.
    OpBuilder<(ins
      "llvm::StringRef":$name,
      "mlir::Type":$mlirType,
      "mlir::Attribute":$typeInfo,
      CArg<"mlir::FlatSymbolRefAttr", "{}">:$base)>
  ];

  let hasCustomAssemblyFormat = 1;
  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// AllocaOp
//===----------------------------------------------------------------------===//

// Allocates stack storage for a single Ada typed variable.
//
// We cannot use memref.alloca for this purpose because
// memref::AllocaOp::getPromotableSlots() gates on isSupportedElementType(),
// which is hardcoded in mlir/lib/IR/Builders.cpp to a fixed set of built-in
// types (FloatType, IndexType, IntegerType, RankedTensorType, VectorType).
// ada.qual matches none of them, so every memref<!ada.qual<T>> alloca is
// silently skipped by mem2reg with no error or warning. An external interface
// model cannot be attached because memref::AllocaOp already owns the
// interface, and attachInterface fails on a duplicate.
//
// This op reimplements the interface without the isSupportedElementType gate,
// letting mem2reg promote ada.qual allocas. Other memref operations
// (memref.load, memref.store) work with !ada.qual<T> element types without
// modification and are still used alongside this op.
def AllocaOp : Ada_Op<"alloca", [
    // Declares the stack-allocation effect (matching memref.alloca) so a dead
    // alloca becomes trivially removable once a DCE/canonicalizer pass runs.
    MemoryEffects<[MemAlloc<AutomaticAllocationScopeResource>]>,
    DeclareOpInterfaceMethods<PromotableAllocationOpInterface>
  ]> {
  let summary = "Ada typed local variable stack allocation";
  let description = [{
    Allocates stack storage for a single Ada typed scalar variable. The result
    is a `memref` whose element type is `!ada.qual<T, @sym>`, preserving Ada
    type identity through the alloca/load/store sequence until mem2reg promotes
    the slot to an SSA value.

    ```mlir
    ada.alloca : memref<!ada.qual<i32, @integer>>
    ```
  }];
  let results = (outs AnyMemRef:$result);
  let assemblyFormat = "attr-dict `:` type($result)";
  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// ConstantOp
//===----------------------------------------------------------------------===//

// `arith.constant` cannot be used directly because its verifier requires
// value.getType() == result.getType(). Standard IntegerAttr/FloatAttr carry
// the inner MLIR type (e.g. i32), not !ada.qual<i32, @sym>, so the verifier
// would reject an ada.qual result. This op relaxes the rule: only the inner
// mlirType component of the result must match the attribute type.
def ConstantOp : Ada_Op<"constant", [Pure, ConstantLike]> {
  let summary = "Ada typed constant value";
  let description = [{
    A constant scalar value annotated with its Ada type identity. The value
    attribute carries a standard `IntegerAttr` or `FloatAttr` whose type
    matches the `mlirType` component of the `!ada.qual` result; the Ada type
    symbol lives on the result type rather than in the attribute. Lowered to
    `arith.constant` by LowerToLLVM, which strips the Ada type wrapper.

    ```mlir
    ada.constant : !ada.qual<i32, @standard.integer> = 42
    ada.constant : !ada.qual<f32, @standard.float> = 1.000000e+00
    ```
  }];
  let arguments = (ins AnyAttr:$value);
  let results = (outs Ada_QualType:$result);
  let hasCustomAssemblyFormat = 1;
  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// CoerceOp
//===----------------------------------------------------------------------===//

// Emitted by MLIRGen at every point where a value flows into a typed location:
// parameter passing, assignment, return, object initialization, and explicit
// type conversions (@rm{4-6}). Lowered to an arith cast when the underlying MLIR
// types differ; a no-op otherwise. Future: will also emit a range check against
// the target type's bounds before the cast, raising Constraint_Error on
// violation.
def CoerceOp : Ada_Op<"coerce", [Pure]> {
  let summary = "Ada value coercion between types";
  let description = [{
    Represents the coercion of a value to a target Ada type (@rm{4-6}). Emitted
    at every point where a value flows into a typed location: explicit type
    conversions, assignments, parameter passing, and return statements.

    When the underlying MLIR types differ, lowers to the appropriate arith
    cast (`arith.trunci`, `arith.extsi`, `arith.extui`, `arith.truncf`,
    `arith.extf`). When the types match (same machine representation, different
    Ada type), lowers to a value forward.

    Example:

    ```mlir
    %1 = ada.coerce %0
        : !ada.qual<f64, @standard.universal_real_type_>
          to !ada.qual<f32, @standard.float>
    ```
  }];
  let arguments = (ins Ada_QualType:$input);
  let results = (outs Ada_QualType:$result);
  let assemblyFormat = "$input attr-dict `:` type($input) `to` type($result)";
  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// RangeOp
//===----------------------------------------------------------------------===//

// A subtype's constraint as a first-class value: the (low, high) bound pair.
// Static bounds are constant operands; dynamic bounds (@rm{3-2-2}) are runtime
// SSA values evaluated once at subtype elaboration. The bounds are bare
// machine values (`iN`/`fN`); the Ada subtype identity lives on the result
// `!ada.range` type, not on the operands. No folder: there is no attribute
// form of a range to fold to (static bound metadata lives on `ada.type`'s
// `int_info` instead); `staticBounds()` exposes the constant operands for the
// `ada.range_check` folder and future `'First`/`'Last`.
// The two `TypesMatchWith` constraints fix each bound operand's type to the
// range's `boundType`. They double as the assembly-format type inference: the
// printed form carries only the result type, and the operand types are derived
// from it, so no custom parser/printer is needed.
def RangeOp : Ada_Op<"range", [Pure,
    TypesMatchWith<"low bound type is the range bound type", "result", "low",
                   "::mlir::cast<::mlir::ada::RangeType>($_self).getBoundType()">,
    TypesMatchWith<"high bound type is the range bound type", "result", "high",
                   "::mlir::cast<::mlir::ada::RangeType>($_self).getBoundType()">]> {
  let summary = "Ada subtype constraint value (a low/high bound pair)";
  let description = [{
    Builds the runtime descriptor of a scalar subtype's range constraint: a
    pair of bound values, packaged as an `!ada.range`. The bounds are bare
    machine values; the constrained Ada subtype is named once on the result
    type. Static bounds are constant operands, dynamic bounds (@rm{3-2-2}) are
    runtime SSA values.

    ```mlir
    %r = ada.range %low, %high : !ada.range<i32, @standard.positive>
    ```
  }];

  let arguments = (ins AnyType:$low, AnyType:$high);
  let results = (outs Ada_RangeType:$result);
  // `qualified` keeps the `!ada.range` prefix: an `ada` op printing an `ada`
  // type would otherwise elide the same-dialect prefix to a bare `<...>`.
  let assemblyFormat = "$low `,` $high attr-dict `:` qualified(type($result))";

  let extraClassDeclaration = [{
    /// The constant values of the two bounds, each null when its operand is
    /// not a compile-time constant. Returned independently so a per-bound
    /// consumer (e.g. folding `'First` alone) can use one side.
    std::pair<mlir::TypedAttr, mlir::TypedAttr> staticBounds();
  }];

  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// RangeCheckOp
//===----------------------------------------------------------------------===//

// Constraint_Check (@rm{11-5}) against a subtype's range, raising
// Constraint_Error on violation. Type-preserving `(T) -> (T)` on the target
// subtype's `!ada.qual`: MLIRGen coerces the value to that subtype first
// (`ada.coerce`) and then checks it, so keeping the result type equal to the
// operand type is what lets the static-pass folder forward the value (a fold
// may only return a value of the result's own type). Not `Pure`: raising is
// observable. The two `TypesMatchWith` constraints pin the value to the result
// type and the range to the matching `!ada.range` (same machine type and
// subtype symbol); they also drive assembly inference, so only the result
// `!ada.qual` is printed.
def RangeCheckOp : Ada_Op<"range_check", [
    TypesMatchWith<"checked value type is the result type", "result", "value",
                   "$_self">,
    TypesMatchWith<"range matches the checked subtype", "result", "range",
                   "::mlir::ada::RangeType::get($_self.getContext(), "
                   "::mlir::cast<::mlir::ada::QualType>($_self).getMlirType(), "
                   "::mlir::cast<::mlir::ada::QualType>($_self).getAdaType())">]> {
  let summary = "Ada Constraint_Check against a subtype range";
  let description = [{
    Checks that `value` lies within `range`, raising `Constraint_Error`
    otherwise (@rm{11-5}). Type-preserving: `value` and the result share the
    target subtype's `!ada.qual` type, and `range` is the matching `!ada.range`
    (same machine type and subtype symbol).

    ```mlir
    %c = ada.range_check %v, %r : !ada.qual<i32, @standard.positive>
    ```
  }];

  let arguments = (ins Ada_QualType:$value, Ada_RangeType:$range);
  let results = (outs Ada_QualType:$result);
  let assemblyFormat =
      "$value `,` $range attr-dict `:` qualified(type($result))";

  let hasFolder = 1;
}

//===----------------------------------------------------------------------===//
// AttrOp
//===----------------------------------------------------------------------===//

// A scalar attribute that reads a bound from a subtype's range descriptor
// (@rm{3-5}): `'First` selects the low bound, `'Last` the high bound. The
// operand is the `!ada.range` descriptor, never the type symbol -- the symbol
// is static representation metadata with no bounds to read, while the
// descriptor carries them (static or dynamic, @rm{3-2-2}). The result is the
// bound as a value of the subtype, so a dynamic bound is read at run time
// (lowered to an `extractvalue`) and a static one folds to a constant.
// The `TypesMatchWith` pins the range to the result subtype's `!ada.range`
// (same machine type and subtype symbol), mirroring `RangeCheckOp`; it also
// drives assembly inference, so only the result `!ada.qual` is printed.
def AttrOp : Ada_Op<"attr", [Pure,
    TypesMatchWith<"range matches the result subtype", "result", "range",
                   "::mlir::ada::RangeType::get($_self.getContext(), "
                   "::mlir::cast<::mlir::ada::QualType>($_self).getMlirType(), "
                   "::mlir::cast<::mlir::ada::QualType>($_self).getAdaType())">]> {
  let summary = "Ada scalar attribute reading a subtype bound (`'First`/`'Last`)";
  let description = [{
    Reads a bound of a scalar subtype's range descriptor (@rm{3-5}): `"first"`
    yields the low bound, `"last"` the high bound, as a value of the subtype.
    The bound may be static or dynamic (@rm{3-2-2}); a dynamic bound is read
    from the descriptor at run time.

    ```mlir
    %last = ada.attr "last", %r : !ada.qual<i32, @dyn.s>
    ```
  }];

  let arguments = (ins StrAttr:$name, Ada_RangeType:$range);
  let results = (outs Ada_QualType:$result);
  let assemblyFormat =
      "$name `,` $range attr-dict `:` qualified(type($result))";

  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// BinOp
//===----------------------------------------------------------------------===//

// SameOperandsAndResultType constrains the op so that all operands and the
// result share one type. It also auto-generates build(AdaBinaryOp, Value, Value)
// that infers the result type from the operands, so no custom builder is needed.
def BinOp : Ada_Op<"binop", [Pure, SameOperandsAndResultType]> {
  let summary = "Ada predefined binary arithmetic operation";
  let description = [{
    Represents an Ada predefined binary arithmetic operation (@rm{4-5-3} to
    @rm{4-5-5}). The operator is encoded as an `AdaBinaryOp` enum attribute.

    `SameOperandsAndResultType` constrains the op so that both operands and the
    result share one type (integer or float). The constraint also auto-generates
    a builder that infers the result type from the operands.

    An optional `checks<...>` group lists the run-time checks the operation
    must perform (`overflow` for signed-integer `+`/`-`/`*`); it is printed only
    when nonempty. Two assembly formats are accepted:

    ```mlir
    %0 = ada.binop "+" %a, %b : i32                       // all types identical
    %0 = ada.binop "+" %a, %b checks<overflow> : i32      // with a check
    %0 = ada.binop "+" %a, %b : (i32, i32) -> i32         // functional form
    ```
  }];

  let arguments = (ins AdaBinaryOp:$kind, AnyType:$lhs, AnyType:$rhs,
                       OptionalAttr<AdaChecks>:$checks);
  let results = (outs AnyType);

  let hasCustomAssemblyFormat = 1;
  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// CmpOp
//===----------------------------------------------------------------------===//

// SameTypeOperands constrains both operands to share one type; the result is
// always Boolean and therefore declared separately. The result type is passed
// explicitly to the builder rather than inferred.
def CmpOp : Ada_Op<"cmp", [Pure, SameTypeOperands]> {
  let summary = "Ada predefined relational operation";
  let description = [{
    Represents an Ada predefined relational operation (@rm{4-5-2}). The operator
    is encoded as an `AdaRelationalOp` enum attribute.

    Both operands share one type (`SameTypeOperands`); the result is the
    predefined type `Boolean`, so the result type differs from the operand type
    and is printed in functional form, matching `ada.binop`'s functional
    spelling:

    ```mlir
    %0 = ada.cmp "=" %a, %b : (i32, i32) -> i1
    ```
  }];

  let arguments = (ins AdaRelationalOp:$kind, AnyType:$lhs, AnyType:$rhs);
  let results = (outs AnyType);

  let hasCustomAssemblyFormat = 1;
  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// UnwrapOp
//===----------------------------------------------------------------------===//

// Exposes the builtin MLIR value underneath an `ada.qual` annotation. Value
// preserving: the result is the same value with the Ada type identity dropped.
// Used to feed an Ada value into builtin/`cf`/`arith` ops that require the raw
// type, e.g. the `i1` condition of a `cf.cond_br`.
def UnwrapOp : Ada_Op<"unwrap", [Pure]> {
  let summary = "Expose the underlying MLIR value of an Ada-qualified value";
  let description = [{
    Converts a value of `ada.qual<T, @sym>` to its underlying MLIR type `T`,
    discarding the Ada type identity. The lowered `ada.qual<T>` is just `T`, so
    this op folds away during lowering.

    ```mlir
    %0 = ada.unwrap %c : !ada.qual<i1, @standard.boolean> to i1
    ```
  }];
  let arguments = (ins AnyType:$value);
  let results = (outs AnyType:$result);
  let assemblyFormat = "$value attr-dict `:` type($value) `to` type($result)";
  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// NullOp
//===----------------------------------------------------------------------===//

def NullOp : Ada_Op<"null", [Pure]> {
  let summary = "null statement: no operation";
  let description = [{
    Represents the Ada null statement (@rm{5-1}), which explicitly does nothing.
    It is used as a placeholder where a statement is syntactically required but
    no action is needed (e.g. an empty procedure body or an empty branch).
    Lowered to nothing by the LowerToLLVM pass.
  }];
  let assemblyFormat = "attr-dict";
}

//===----------------------------------------------------------------------===//
// DeclsOp
//===----------------------------------------------------------------------===//

// Enclosing SymbolTable for a subprogram's nested symbol declarations, since
// `ada.subp` itself is not one. See the op description for details.
def DeclsOp : Ada_Op<"decls", [NoTerminator, SingleBlock, SymbolTable,
                               HasParent<"::mlir::ada::SubpOp">]> {
  let summary = "Container for a subprogram's nested symbol declarations";
  let description = [{
    A `SymbolTable` region holding the nested symbol declarations (`ada.subp`,
    `ada.type`) of a subprogram's declarative part. `ada.subp` is not itself a
    `SymbolTable` (its body is a multi-block CFG), so its nested symbols need
    some enclosing `SymbolTable`; this op provides one inside the subprogram
    body. It carries no operands or results and is not `IsolatedFromAbove`, so
    enclosing SSA values pass through it. `HoistNestedSymbolOperations` lifts the
    contents to module level and erases the emptied op before lowering, so it
    never reaches `LowerToLLVM`.

    ```mlir
    ada.subp @foo() {
      ada.decls {
        ada.subp @"foo.bar"() { ada.return }
        ada.type @"foo.color" : i1 = #ada.enum_info<"red" = 0, "green" = 1>
      }
      ada.return
    }
    ```
  }];
  let regions = (region AnyRegion:$body);
  let assemblyFormat = "$body attr-dict";
}

//===----------------------------------------------------------------------===//
// CallOp
//===----------------------------------------------------------------------===//

// Subprogram call. The callee is a flat symbol resolved by walking all
// enclosing SymbolTable scopes (e.g. DeclsOp, ModuleOp) rather than
// stopping at the nearest one.
def CallOp : Ada_Op<"call",
                    [CallOpInterface,
                     DeclareOpInterfaceMethods<SymbolUserOpInterface>]> {
  let summary = "subprogram call";
  let description = [{
    Represents an Ada subprogram call. Procedure calls carry no result;
    function calls carry exactly one.

    Before lowering, `HoistNestedSymbolOperations` lifts nested subprograms to
    module level, renames them with GNAT-style `__`-separated qualifiers, and
    rewrites all call references accordingly. `LowerToLLVM` then converts each
    `ada.call` to a `func.call`.

    Syntax example:

    ```mlir
    ada.call @add(%a, %b) : (i32, i32) -> i32  // function call
    ada.call @print(%x) : (i32) -> ()          // procedure call
    ```
  }];
  let arguments = (ins
    FlatSymbolRefAttr:$callee,              // callee Ada subprogram name
    Variadic<AnyType>:$operands,            // actual call arguments
    OptionalAttr<DictArrayAttr>:$arg_attrs, // per-argument call-site attributes
    OptionalAttr<DictArrayAttr>:$res_attrs  // per-result call-site attributes
  );
  // absent for procedures, present for functions
  let results = (outs Optional<AnyType>:$result);
  let assemblyFormat = "$callee `(` $operands `)` attr-dict"
                       " `:` functional-type($operands, results)";

  let skipDefaultBuilders = 1;
  let builders = [
    // Procedure call: resultType is null. Function call: resultType is non-null.
    OpBuilder<(ins "::mlir::FlatSymbolRefAttr":$callee,
      "::mlir::Type":$resultType,
      CArg<"::mlir::ValueRange", "{}">:$operands)>
  ];

  let extraClassDeclaration = [{
    // CallOpInterface
    mlir::CallInterfaceCallable getCallableForCallee() {
      return getCalleeAttr();
    }
    void setCalleeFromCallable(mlir::CallInterfaceCallable callee) {
      setCalleeAttr(mlir::cast<mlir::FlatSymbolRefAttr>(
          mlir::cast<mlir::SymbolRefAttr>(callee)));
    }
    operand_range getArgOperands() { return getOperands(); }
    mlir::MutableOperandRange getArgOperandsMutable() {
      return getOperandsMutable();
    }

    /// Walk enclosing SymbolTable scopes from `from` upward and return the
    /// first symbol named `name`, or null.
    static mlir::Operation *lookupCallee(mlir::Operation *from,
                                         llvm::StringRef name);
  }];
}

//===----------------------------------------------------------------------===//
// SubpOp
//===----------------------------------------------------------------------===//

// A single op for both Ada functions and procedures. Functions have one result
// type; procedures have an empty result list. The op is NOT IsolatedFromAbove:
// a nested subprogram may reference an enclosing variable as an ordinary
// cross-region SSA use (an up-level reference). The `ClosureConversion` pass
// lambda-lifts those captures into explicit parameters and hoists the
// subprogram to module level, so it is self-contained (and effectively
// isolated again) by the time `LowerToLLVM` turns it into a `func.func` (LLVM
// does not support nested functions). The op is also not a SymbolTable: its
// body is a multi-block CFG (for control flow), so nested symbol declarations
// live in an interior `ada.decls` (a SymbolTable) rather than directly in the
// body. AutomaticAllocationScope marks the op as the alloca scope boundary for
// mem2reg.
def SubpOp : Ada_Op<"subp", [
    FunctionOpInterface, AutomaticAllocationScope
  ]> {
  let summary = "Ada subprogram (function or procedure)";
  let description = [{
    Represents an Ada subprogram. Functions carry one result type; procedures
    have an empty result list. The distinction is entirely in the function type.

    Example:

    ```mlir
    ada.subp @add(%a: i32, %b: i32) -> i32 {
      %r = ada.binop "+" %a, %b : i32
      ada.return %r : i32
    }
    ada.subp @print(%x: i32) {
      ada.return
    }
    ```
  }];

  let arguments = (ins
    SymbolNameAttr:$sym_name,
    TypeAttrOf<FunctionType>:$function_type,
    OptionalAttr<DictArrayAttr>:$arg_attrs,
    OptionalAttr<DictArrayAttr>:$res_attrs
  );
  let regions = (region AnyRegion:$body);

  let builders = [OpBuilder<(ins
    "StringRef":$name, "FunctionType":$type,
    CArg<"ArrayRef<NamedAttribute>", "{}">:$attrs)
  >];

  let extraClassDeclaration = [{
    ArrayRef<Type> getArgumentTypes() { return getFunctionType().getInputs(); }
    ArrayRef<Type> getResultTypes() { return getFunctionType().getResults(); }
    Region *getCallableRegion() { return &getBody(); }
    bool isProcedure() { return getFunctionType().getNumResults() == 0; }
    bool isFunction() { return getFunctionType().getNumResults() == 1; }
    /// GNAT ABI name, derived from the qualified sym_name: split on the dot,
    /// operator-map each segment, and join with double underscores (e.g.
    /// proc.b.inner to proc__b__inner). Library-level subprograms get the
    /// _ada_ prefix. Reads symbol visibility rather than the op's parent, so
    /// the name stays stable across passes that hoist the op to module level.
    std::string getMangledName();
  }];

  let hasCustomAssemblyFormat = 1;
  // skipDefaultBuilders: the ODS-generated default builder doesn't know how to
  // create the entry block, so we provide our own via buildWithEntryBlock.
  let skipDefaultBuilders = 1;
}

//===----------------------------------------------------------------------===//
// ReturnOp
//===----------------------------------------------------------------------===//

// @todo Once LLVM 23.1 ships `HasAncestor` in ODS, switch `HasParent` to
// `HasAncestor<"ada.subp">`, which also covers a return nested in a sub-region
// (e.g. a future block statement); `HasParent` only covers today's flat CFG.
//
// Terminator marks this op as the mandatory last op in a basic block.
def ReturnOp : Ada_Op<"return", [Terminator, ReturnLike,
                                 HasParent<"::mlir::ada::SubpOp">]> {
  let summary = "return operation";
  let description = [{
    Represents an Ada return statement (@rm{5-8}). Functions carry one return
    operand whose type must match the subprogram result type; procedures carry
    none.

    ```mlir
    ada.return %0 : i32  // function return
    ada.return           // procedure return
    ```
  }];

  let arguments = (ins Optional<AnyType>:$input);

  let assemblyFormat = "($input^ `:` type($input))? attr-dict";

  let hasVerifier = 1;
}

#endif // ADA_OPS
