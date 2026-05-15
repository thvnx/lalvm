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

include "mlir/IR/OpBase.td"
include "mlir/IR/EnumAttr.td"
include "mlir/Interfaces/FunctionInterfaces.td"
include "mlir/IR/SymbolInterfaces.td"
include "mlir/Interfaces/SideEffectInterfaces.td"

// C++ implementation in mlir/IR/OpDefinition.h.
def SameOperandsAndResultType : NativeOpTrait<"SameOperandsAndResultType">;

// Provide a definition of the 'Ada' dialect in the ODS framework so that we
// can define our operations.
def Ada_Dialect : Dialect {
  let name = "ada";
  let cppNamespace = "::mlir::ada";
}

// Base class for all Ada dialect operations. Binds each op to the Ada dialect
// and its mnemonic (the part after the "ada." prefix in MLIR text form).
class Ada_Op<string mnemonic, list<Trait> traits = []> :
    Op<Ada_Dialect, mnemonic, traits>;

//===----------------------------------------------------------------------===//
// Ada Operations
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// BinOp
//===----------------------------------------------------------------------===//

def AdaBinaryOp_Add : I32EnumAttrCase<"Add", 0, "+">;
def AdaBinaryOp_Sub : I32EnumAttrCase<"Sub", 1, "-">;
def AdaBinaryOp_Mul : I32EnumAttrCase<"Mul", 2, "*">;

def AdaBinaryOp : I32EnumAttr<"AdaBinaryOp",
    "Ada predefined binary operator kind",
    [AdaBinaryOp_Add, AdaBinaryOp_Sub, AdaBinaryOp_Mul]> {
  let cppNamespace = "::mlir::ada";
}

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
// ada.func/ada.proc, this op is NOT IsolatedFromAbove — SSA values from the
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
// nearest enclosing SymbolTable, which may be an ada.func/ada.proc for nested
// calls). The LowerToLLVM pass hoists nested subprograms to module level and
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
// FuncOp
//===----------------------------------------------------------------------===//

// FunctionOpInterface provides the standard function-like behaviour (argument
// types, result types, callable region). IsolatedFromAbove prevents the region
// from capturing SSA values defined outside the function, matching Ada's
// scoping rules and enabling independent analysis/optimization of each function.
// SymbolTable makes each function op a symbol scope so that nested subprogram
// bodies declared inside it are scoped to it rather than to the module.
def FuncOp : Ada_Op<"func", [
    FunctionOpInterface, IsolatedFromAbove, SymbolTable
  ]> {
  let summary = "user defined function operation";
  let description = [{
    The "ada.func" operation represents a user defined function. These are
    callable SSA-region operations that contain Ada computations.

    Example:

    ```mlir
    ada.func @main() -> f32 {
      %0 = ada.constant 5.500000e+00 : f32
      ada.return %0 : f32
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
    //===------------------------------------------------------------------===//
    // FunctionOpInterface Methods
    //===------------------------------------------------------------------===//

    /// Returns the argument types of this function.
    ArrayRef<Type> getArgumentTypes() { return getFunctionType().getInputs(); }

    /// Returns the result types of this function.
    ArrayRef<Type> getResultTypes() { return getFunctionType().getResults(); }

    Region *getCallableRegion() { return &getBody(); }
  }];

  let hasCustomAssemblyFormat = 1;
  // skipDefaultBuilders: the ODS-generated default builder doesn't know how to
  // create the entry block, so we provide our own via buildWithEntryBlock.
  let skipDefaultBuilders = 1;
}

//===----------------------------------------------------------------------===//
// ProcOp
//===----------------------------------------------------------------------===//

// Ada procedures are like functions but with no return value. We use a separate
// op (rather than a FuncOp with empty result list) so that the verifier can
// enforce that ada.return inside a procedure carries no operand. That
// enforcement is implemented in ReturnOp::verify, which checks that the number
// of return operands matches the enclosing function's result count (zero for
// a procedure). SymbolTable is included for the same reason as FuncOp: nested
// subprogram bodies are scoped to the enclosing subprogram.
def ProcOp : Ada_Op<"proc", [
    FunctionOpInterface, IsolatedFromAbove, SymbolTable
  ]> {
  let summary = "user defined procedure operation";

  // Same layout as FuncOp: required by FunctionOpInterface.
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
  }];

  let hasCustomAssemblyFormat = 1;
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
      ada.func @foo() -> i32 {
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
