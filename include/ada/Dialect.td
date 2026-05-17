//===- Dialect.td - Ada dialect definition ------------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines the Ada dialect and the Ada_Op base class shared by Attrs.td and
// Ops.td.
//
//===----------------------------------------------------------------------===//

#ifndef ADA_DIALECT
#define ADA_DIALECT

include "mlir/IR/OpBase.td"

// Provide a definition of the 'Ada' dialect in the ODS framework so that we
// can define our operations and attributes.
def Ada_Dialect : Dialect {
  let name = "ada";
  let cppNamespace = "::mlir::ada";
  let useDefaultAttributePrinterParser = 1;
}

// Base class for all Ada dialect operations. Binds each op to the Ada dialect
// and its mnemonic (the part after the "ada." prefix in MLIR text form).
class Ada_Op<string mnemonic, list<Trait> traits = []> :
    Op<Ada_Dialect, mnemonic, traits>;

#endif // ADA_DIALECT
