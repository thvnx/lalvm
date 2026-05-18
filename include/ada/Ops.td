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

// C++ implementation in mlir/IR/OpDefinition.h.
def SameOperandsAndResultType : NativeOpTrait<"SameOperandsAndResultType">;

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
// Binary adding operators (RM 4.5.3)
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
// BinOp
//===----------------------------------------------------------------------===//

def AdaBinaryOp : I32EnumAttr<"AdaBinaryOp",
    "Ada predefined binary operator kind",
    [AdaOp_Plus, AdaOp_Minus, AdaOp_Mult, AdaOp_Div]> {
  let cppNamespace = "::mlir::ada";
}

//===----------------------------------------------------------------------===//
// TypeOp
//===----------------------------------------------------------------------===//

// Represents an Ada type declaration. Metadata-only op with no runtime value:
// its sole purpose is to carry type information (name, MLIR type, kind-specific
// data) through the pipeline for DWARF debug info generation. Currently erased
// during lowering; full DWARF emission is not yet implemented.
//
// type_info holds an EnumTypeInfoAttr for enumeration types. Future kinds
// (integer, float, record, array, ...) will use additional AttrDef subclasses.
//
// Implements Symbol so that ada.constant ops can reference it by name via
// FlatSymbolRefAttr. Local types live inside the enclosing ada.subp region;
// external types are emitted at module level.
def TypeOp : Ada_Op<"type", [Symbol]> {
  let summary = "Ada type declaration (metadata only, for DWARF debug info)";
  let arguments = (ins
    SymbolNameAttr:$sym_name,
    TypeAttr:$mlir_type,
    AnyAttr:$type_info
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
// ConstantOp
//===----------------------------------------------------------------------===//

// Represents an Ada typed constant: an enum literal use or a constant object
// declaration (RM 3.3). Carries the enumerator rep value as an integer
// attribute and a symbol reference to the corresponding ada.type op so that
// lowering can reconstruct the DWARF type.
//
// The Ada name of the constant or literal is carried in the op location as a
// NameLoc: "C" for `C : constant Color := Green`, "green" for an anonymous
// use such as `return Green`.
def ConstantOp : Ada_Op<"constant", [Pure,
    DeclareOpInterfaceMethods<SymbolUserOpInterface>]> {
  let summary = "Ada typed constant (enum literal or constant object declaration)";
  let arguments = (ins
    APIntAttr:$value,
    FlatSymbolRefAttr:$ada_type
  );
  let results = (outs AnyInteger:$result);

  let skipDefaultBuilders = 1;
  let builders = [
    OpBuilder<(ins
      "mlir::IntegerAttr":$value,
      "llvm::StringRef":$adaType)>
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

  let arguments = (ins AdaBinaryOp:$kind, AnyType:$lhs, AnyType:$rhs);
  let results = (outs AnyType);

  let hasCustomAssemblyFormat = 1;
  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// NopOp
//===----------------------------------------------------------------------===//

// Ada null statement (RM 5.1): a no-operation statement. Lowered to nothing.
def NullOp : Ada_Op<"null", [Pure]> {
  let summary = "null statement — no operation";
  let description = [{
    Represents the Ada null statement (RM 5.1), which explicitly does nothing.
    It is used as a placeholder where a statement is syntactically required but
    no action is needed (e.g. an empty procedure body or an empty branch).
    Lowered to nothing by the LowerToLLVM pass.
  }];
  let assemblyFormat = "attr-dict";
}

//===----------------------------------------------------------------------===//
// BlockStmtOp
//===----------------------------------------------------------------------===//

// Represents an Ada block statement (RM 5.6): an inline scope that may
// introduce local declarations followed by a sequence of statements. Unlike
// ada.subp, this op is NOT IsolatedFromAbove — SSA values from the
// enclosing function are directly accessible. NoTerminator+SingleBlock waive
// the need for an explicit terminator op. During lowering the region contents
// are inlined into the parent block and the op is erased.
def BlockStmtOp : Ada_Op<"block_stmt", [NoTerminator, SingleBlock]> {
  let summary = "Ada block statement with its own declarative scope";
  let arguments = (ins OptionalAttr<StrAttr>:$name);
  let regions = (region AnyRegion:$body);
  let assemblyFormat = "($name^)? $body attr-dict";
}

//===----------------------------------------------------------------------===//
// CallOp
//===----------------------------------------------------------------------===//

// Subprogram call. Procedure calls carry no result; function calls carry
// exactly one. The callee is referenced by a flat symbol (resolved via the
// nearest enclosing SymbolTable, which may be an ada.subp for nested calls).
// The LowerToLLVM pass hoists nested subprograms to module level and
// updates all call references accordingly before converting ada.call to
// func.call.
def CallOp : Ada_Op<"call", []> {
  let summary = "subprogram call";
  let arguments = (ins FlatSymbolRefAttr:$callee, Variadic<AnyType>:$operands);
  let results = (outs Optional<AnyType>:$result);
  let assemblyFormat = "$callee `(` $operands `)` attr-dict"
                       " `:` functional-type($operands, results)";

  let skipDefaultBuilders = 1;
  let builders = [
    // Procedure call (no result).
    OpBuilder<(ins "::llvm::StringRef":$callee,
      CArg<"::mlir::ValueRange", "{}">:$operands)>,
    // Function call (one result type).
    OpBuilder<(ins "::llvm::StringRef":$callee, "::mlir::Type":$resultType,
      CArg<"::mlir::ValueRange", "{}">:$operands)>
  ];

}

//===----------------------------------------------------------------------===//
// SubpOp
//===----------------------------------------------------------------------===//

// A single op for both Ada functions and procedures. Functions have one result
// type; procedures have an empty result list. IsolatedFromAbove prevents the
// region from capturing SSA values defined outside the subprogram, matching
// Ada's scoping rules. SymbolTable scopes nested subprogram bodies to the
// enclosing subprogram rather than to the module.
def SubpOp : Ada_Op<"subp", [
    FunctionOpInterface, IsolatedFromAbove, SymbolTable
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
// Pure is intentionally absent: terminators transfer control flow and must
// not be eliminated by dead-code elimination.
def ReturnOp : Ada_Op<"return", [Terminator]> {
  let summary = "return operation";
  let description = [{
    The "return" operation represents a return operation within a function.
    The operation takes an operand and produces no results.
    The operand type must match the signature of the function that contains
    the operation. For example:

    ```mlir
      ada.subp @foo() -> i32 {
        ...
        ada.return %0 : i32
      }
    ```
  }];

  // The return operation takes an input operand to return. This value must
  // match the return type of the enclosing function.
  let arguments = (ins Variadic<AnyType>:$input);

  let hasCustomAssemblyFormat = 1;

  // Provide extra utility definitions on the c++ operation class definition.
  let extraClassDeclaration = [{
    bool hasOperand() { return getNumOperands() != 0; }
  }];

  // Invoke a static verify method to verify this return operation.
  let hasVerifier = 1;
}

#endif // ADA_OPS
