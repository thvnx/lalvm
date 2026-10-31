// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=mlir %s | %FileCheck %s
// RUN: %lalvm --emit=mlir -g %s | %FileCheck %s
// RUN: %lalvm --emit=mlir --mlir-use-nameloc-as-prefix=false %s | %FileCheck %s --check-prefix=NUMBERED

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>

  // CHECK-LABEL: ada.subp @names(%input:
  // NUMBERED-LABEL: ada.subp @names(%arg0:
  ada.subp @names(%arg: !ada.qual<i32, @integer> loc("input")) -> !ada.qual<i32, @integer> {
    // CHECK: %zero = ada.constant
    // NUMBERED: %0 = ada.constant
    %zero = ada.constant : !ada.qual<i32, @integer> = 0 loc("zero")
    // CHECK: %x = ada.alloca
    %slot = ada.alloca : memref<!ada.qual<i32, @integer>> loc("x")
    memref.store %arg, %slot[] : memref<!ada.qual<i32, @integer>>
    // Names apply across dialects and collisions are disambiguated.
    // CHECK: %[[LOADED:x_[0-9]+]] = memref.load %x[]
    %loaded = memref.load %slot[] : memref<!ada.qual<i32, @integer>> loc("x")
    // Nested NameLoc metadata also provides a name.
    // CHECK: %sum = ada.binop "+" %[[LOADED]], %zero
    %sum = ada.binop "+" %loaded, %zero : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer> loc(fused["sum"])
    %condition = arith.constant true
    // Multiple results retain their result group and use the source name.
    // CHECK: %pair:2 = scf.if
    %pair:2 = scf.if %condition -> (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) {
      scf.yield %sum, %zero : !ada.qual<i32, @integer>, !ada.qual<i32, @integer>
    } else {
      scf.yield %zero, %sum : !ada.qual<i32, @integer>, !ada.qual<i32, @integer>
    } loc("pair")
    // CHECK: ada.return %pair#0
    ada.return %pair#0 : !ada.qual<i32, @integer>
  }
}
