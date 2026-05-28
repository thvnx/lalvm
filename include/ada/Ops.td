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
include "mlir/IR/EnumAttr.td"
include "mlir/Interfaces/FunctionInterfaces.td"
include "mlir/IR/SymbolInterfaces.td"
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
// NOTE: `Symbol` trait cannot be used here. `ada.type` ops are marked legal
// in `LowerToLLVM` and survive into `llvm.func` bodies, whose parent op
// (`LLVM::LLVMFuncOp`) does not carry `SymbolTable`. The MLIR verifier checks
// this invariant after every pass, so `Symbol` would always fail post-lowering.
def TypeOp : Ada_Op<"type", []> {
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
  let arguments = (ins OptionalAttr<StrAttr>:$name);
  let regions = (region AnyRegion:$body);
  let assemblyFormat = "($name^)? $body attr-dict";
}

//===----------------------------------------------------------------------===//
// CallOp
//===----------------------------------------------------------------------===//

// Subprogram call. The callee is a flat symbol resolved by walking all
// enclosing SymbolTable scopes (e.g. BlockOp, SubpOp, ModuleOp) rather than
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
// straightforward (LLVM does not support nested functions). SymbolTable
// scopes nested subprogram bodies to the enclosing subprogram rather than
// to the module. AutomaticAllocationScope marks the op as the alloca scope
// boundary for mem2reg.
def SubpOp : Ada_Op<"subp", [
    FunctionOpInterface, IsolatedFromAbove, SymbolTable,
    AutomaticAllocationScope
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
    /// ABI renaming contract (matches LowerToLLVM):
    ///   - library-level: "_ada_" + bare name
    ///   - nested: "outer__inner" (no _ada_ prefix)
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
