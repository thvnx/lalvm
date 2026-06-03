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

// Logical operators (RM 4.5.1)
def AdaOp_And     : I32EnumAttrCase<"And",     0,  "and">;
def AdaOp_Or      : I32EnumAttrCase<"Or",      1,  "or">;
def AdaOp_Xor     : I32EnumAttrCase<"Xor",     2,  "xor">;
def AdaOp_AndThen : I32EnumAttrCase<"AndThen", 3,  "and then">;
def AdaOp_OrElse  : I32EnumAttrCase<"OrElse",  4,  "or else">;
// Relational operators (RM 4.5.2)
def AdaOp_Eq      : I32EnumAttrCase<"Eq",      5,  "=">;
def AdaOp_Neq     : I32EnumAttrCase<"Neq",     6,  "/=">;
def AdaOp_Lt      : I32EnumAttrCase<"Lt",      7,  "<">;
def AdaOp_Lte     : I32EnumAttrCase<"Lte",     8,  "<=">;
def AdaOp_Gt      : I32EnumAttrCase<"Gt",      9,  ">">;
def AdaOp_Gte     : I32EnumAttrCase<"Gte",     10, ">=">;
def AdaOp_In      : I32EnumAttrCase<"In",      11, "in">;
def AdaOp_NotIn   : I32EnumAttrCase<"NotIn",   12, "not in">;
// Binary and unary adding operators (RM 4.5.3/RM 4.5.4)
def AdaOp_Plus    : I32EnumAttrCase<"Plus",    13, "+">;
def AdaOp_Minus   : I32EnumAttrCase<"Minus",   14, "-">;
def AdaOp_Concat  : I32EnumAttrCase<"Concat",  15, "&">;
// Multiplying operators (RM 4.5.5)
def AdaOp_Mult    : I32EnumAttrCase<"Mult",    16, "*">;
def AdaOp_Div     : I32EnumAttrCase<"Div",     17, "/">;
def AdaOp_Mod     : I32EnumAttrCase<"Mod",     18, "mod">;
def AdaOp_Rem     : I32EnumAttrCase<"Rem",     19, "rem">;
// Highest precedence operators (RM 4.5.6)
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

    `type_info` is one of the Ada dialect type info attributes.

    Syntax example:

    ```mlir
    ada.type @Color : i8 = #ada.enum_info<"red" = 0, "green" = 1, "blue" = 2>
    ada.type @Integer : i32 = #ada.numeric_info
    ```
  }];
  let arguments = (ins
    SymbolNameAttr:$sym_name,  // Ada type declaration name
    TypeAttr:$mlir_type,       // Corresponding MLIR type
    AnyAttr:$type_info         // Kind-specific metadata
  );

  let skipDefaultBuilders = 1;
  let builders = [
    OpBuilder<(ins
      "llvm::StringRef":$name,
      "mlir::Type":$mlirType,
      "mlir::Attribute":$typeInfo)>
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
// type conversions (RM 4.6). Lowered to an arith cast when the underlying MLIR
// types differ; a no-op otherwise. Future: will also emit a range check against
// the target type's bounds before the cast, raising Constraint_Error on
// violation.
def CoerceOp : Ada_Op<"coerce", [Pure]> {
  let summary = "Ada value coercion between types";
  let description = [{
    Represents the coercion of a value to a target Ada type (RM 4.6). Emitted
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
// BinOp
//===----------------------------------------------------------------------===//

// SameOperandsAndResultType constrains the op so that all operands and the
// result share one type. It also auto-generates build(AdaBinaryOp, Value, Value)
// that infers the result type from the operands, so no custom builder is needed.
def BinOp : Ada_Op<"binop", [Pure, SameOperandsAndResultType]> {
  let summary = "Ada predefined binary arithmetic operation";
  let description = [{
    Represents an Ada predefined binary arithmetic operation (RM 4.5.3-4.5.5).
    The operator is encoded as an `AdaBinaryOp` enum attribute.

    `SameOperandsAndResultType` constrains the op so that both operands and the
    result share one type (integer or float). The constraint also auto-generates
    a builder that infers the result type from the operands.

    Two assembly formats are accepted:

    ```mlir
    %0 = ada.binop "+" %a, %b : i32              // all types identical
    %0 = ada.binop "+" %a, %b : (i32, i32) -> i32  // functional form
    ```
  }];

  let arguments = (ins AdaBinaryOp:$kind, AnyType:$lhs, AnyType:$rhs);
  let results = (outs AnyType);

  let hasCustomAssemblyFormat = 1;
  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// NullOp
//===----------------------------------------------------------------------===//

def NullOp : Ada_Op<"null", [Pure]> {
  let summary = "null statement: no operation";
  let description = [{
    Represents the Ada null statement (RM 5.1), which explicitly does nothing.
    It is used as a placeholder where a statement is syntactically required but
    no action is needed (e.g. an empty procedure body or an empty branch).
    Lowered to nothing by the LowerToLLVM pass.
  }];
  let assemblyFormat = "attr-dict";
}

//===----------------------------------------------------------------------===//
// BlockOp
//===----------------------------------------------------------------------===//

// Represents an Ada block statement (RM 5.6): an inline scope that may
// introduce local declarations followed by a sequence of statements. Unlike
// ada.subp, this op is NOT IsolatedFromAbove; SSA values from the
// enclosing function are directly accessible. NoTerminator+SingleBlock waive
// the need for an explicit terminator op. During lowering the region contents
// are inlined into the parent block and the op is erased.
def BlockOp : Ada_Op<"block", [NoTerminator, SingleBlock, SymbolTable]> {
  let summary = "Ada block statement with its own declarative scope";
  // sym_name is the unique qualified dialect symbol, built from the enclosing
  // scope and the block label (or `b` for unnamed blocks).
  let arguments = (ins SymbolNameAttr:$sym_name);
  let regions = (region AnyRegion:$body);
  let assemblyFormat = "$sym_name $body attr-dict";
}

//===----------------------------------------------------------------------===//
// DeclsOp
//===----------------------------------------------------------------------===//

// Enclosing SymbolTable for a subprogram's nested symbol declarations, since
// `ada.subp` itself is not one. See the op description for details.
def DeclsOp : Ada_Op<"decls", [NoTerminator, SingleBlock, SymbolTable]> {
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
        ada.type @"foo.color" : i8 = #ada.enum_info<"red" = 0, "green" = 1>
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
// enclosing SymbolTable scopes (e.g. BlockOp, DeclsOp, ModuleOp) rather than
// stopping at the nearest one.
def CallOp : Ada_Op<"call",
                    [CallOpInterface,
                     DeclareOpInterfaceMethods<SymbolUserOpInterface>]> {
  let summary = "subprogram call";
  let description = [{
    Represents an Ada subprogram call. Procedure calls carry no result;
    function calls carry exactly one.

    Before lowering, `LowerToLLVM` hoists nested subprograms to module level,
    renames them with GNAT-style `__`-separated qualifiers, and rewrites all
    call references accordingly. Each `ada.call` is then converted to a
    `func.call`.

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
// type; procedures have an empty result list. IsolatedFromAbove forces all
// data flow into nested subprograms to be explicit; upward references to
// outer variables must be threaded via a static link rather than captured
// as SSA values. This makes hoisting to module level in LowerToLLVM
// straightforward (LLVM does not support nested functions). The op is not a
// SymbolTable: its body is a multi-block CFG (for control flow), so nested
// symbol declarations live in an interior `ada.decls` (a SymbolTable) rather
// than directly in the body. AutomaticAllocationScope marks the op as the
// alloca scope boundary for mem2reg.
def SubpOp : Ada_Op<"subp", [
    FunctionOpInterface, IsolatedFromAbove, AutomaticAllocationScope
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
    /// _ada_ prefix. Relies on the op's parent, so call before hoisting.
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

// @todo Once LLVM 23.1 is used, add HasAncestor<"ada.subp"> to ReturnOp to get
// free ancestry verification. ReturnOp::verify() can then drop its parent-walk
// and null check.

// Terminator marks this op as the mandatory last op in a basic block.
def ReturnOp : Ada_Op<"return", [Terminator, ReturnLike]> {
  let summary = "return operation";
  let description = [{
    Represents an Ada return statement (RM 5.8). Functions carry one return
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
