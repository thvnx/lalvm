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

// Base class for all Ada dialect operations. Binds each op to the Ada dialect
// and its mnemonic (the part after the "ada." prefix in MLIR text form).
class Ada_Op<string mnemonic, list<Trait> traits = []> :
    Op<Ada_Dialect, mnemonic, traits>;

//===----------------------------------------------------------------------===//
// Ada Operations
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// AddOp
//===----------------------------------------------------------------------===//

// SameOperandsAndResultType constrains the op so that all operands and the
// result share one type. It also auto-generates build(Value lhs, Value rhs)
// that infers the result type from the operands, so no custom builder is needed.
def AddOp : Ada_Op<"add", [Pure, SameOperandsAndResultType]> {
  let summary = "addition operation";

  let arguments = (ins AnyType:$lhs, AnyType:$rhs);
  let results = (outs AnyType);

  let hasCustomAssemblyFormat = 1;
}

//===----------------------------------------------------------------------===//
// SubOp
//===----------------------------------------------------------------------===//

def SubOp : Ada_Op<"sub", [Pure, SameOperandsAndResultType]> {
  let summary = "subtraction operation";

  let arguments = (ins AnyType:$lhs, AnyType:$rhs);
  let results = (outs AnyType);

  let hasCustomAssemblyFormat = 1;
}

//===----------------------------------------------------------------------===//
// FuncOp
//===----------------------------------------------------------------------===//

// FunctionOpInterface provides the standard function-like behaviour (argument
// types, result types, callable region). IsolatedFromAbove prevents the region
// from capturing SSA values defined outside the function, matching Ada's
// scoping rules and enabling independent analysis/optimization of each function.
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
  // skipDefaultBuilders: the ODS-generated default builder doesn't know how to
  // create the entry block, so we provide our own via buildWithEntryBlock.
  let skipDefaultBuilders = 1;
}

//===----------------------------------------------------------------------===//
// ProcOp
//===----------------------------------------------------------------------===//

// Ada procedures are like functions but with no return value. We use a separate
// op (rather than a FuncOp with empty result list) so that the verifier can
// enforce that ada.return inside a procedure carries no operand.
def ProcOp : Ada_Op<"proc", [
    FunctionOpInterface, IsolatedFromAbove
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
// MulOp
//===----------------------------------------------------------------------===//

def MulOp : Ada_Op<"mul", [Pure, SameOperandsAndResultType]> {
  let summary = "multiplication operation";

  let arguments = (ins AnyType:$lhs, AnyType:$rhs);
  let results = (outs AnyType);

  let hasCustomAssemblyFormat = 1;
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
