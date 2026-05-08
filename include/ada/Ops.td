//===- Ops.td - Toy dialect operation definitions ----------*- tablegen -*-===//
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

// Base class for toy dialect operations. This operation inherits from the base
// `Op` class in OpBase.td, and provides:
//   * The parent dialect of the operation.
//   * The mnemonic for the operation, or the name without the dialect prefix.
//   * A list of traits for the operation.
class Ada_Op<string mnemonic, list<Trait> traits = []> :
    Op<Ada_Dialect, mnemonic, traits>;

//===----------------------------------------------------------------------===//
// Ada Operations
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// ConstantOp
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// AddOp
//===----------------------------------------------------------------------===//

def AddOp : Ada_Op<"add", [SameOperandsAndResultType]> {
  let summary = "addition operation";

  let arguments = (ins AnyType:$lhs, AnyType:$rhs);
  let results = (outs AnyType);

  let hasCustomAssemblyFormat = 1;
  // No custom builder: SameOperandsAndResultType auto-generates
  // build(Value lhs, Value rhs) inferring result type from operands.
}

//===----------------------------------------------------------------------===//
// SubOp
//===----------------------------------------------------------------------===//

def SubOp : Ada_Op<"sub", [SameOperandsAndResultType]> {
  let summary = "subtraction operation";

  let arguments = (ins AnyType:$lhs, AnyType:$rhs);
  let results = (outs AnyType);

  let hasCustomAssemblyFormat = 1;
  // No custom builder: SameOperandsAndResultType auto-generates
  // build(Value lhs, Value rhs) inferring result type from operands.
}

//===----------------------------------------------------------------------===//
// FuncOp
//===----------------------------------------------------------------------===//

def FuncOp : Ada_Op<"func", [
    FunctionOpInterface, IsolatedFromAbove
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
  let skipDefaultBuilders = 1;
}

//===----------------------------------------------------------------------===//
// ProcOp
//===----------------------------------------------------------------------===//

def ProcOp : Ada_Op<"proc", [
    FunctionOpInterface, IsolatedFromAbove
  ]> {
  let summary = "user defined procedure operation";

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
// MulOp
//===----------------------------------------------------------------------===//

def MulOp : Ada_Op<"mul", [SameOperandsAndResultType]> {
  let summary = "multiplication operation";

  let arguments = (ins AnyType:$lhs, AnyType:$rhs);
  let results = (outs AnyType);

  let hasCustomAssemblyFormat = 1;
  // No custom builder: SameOperandsAndResultType auto-generates
  // build(Value lhs, Value rhs) inferring result type from operands.
}

//===----------------------------------------------------------------------===//
// ReturnOp
//===----------------------------------------------------------------------===//

def ReturnOp : Ada_Op<"return", [Pure, Terminator]> {
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
