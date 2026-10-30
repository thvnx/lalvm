// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-references %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWER --implicit-check-not=!ada.ref --implicit-check-not=unrealized_conversion_cast
// RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
// RUN: %lalvm --emit=llvm -O1 %s | %FileCheck %s --check-prefix=LLVM

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>

  // LOWER-LABEL: ada.subp @_ada_region
  // LOWER: %[[SELECTED:.*]] = scf.if %arg0 -> (memref<!ada.qual<i32, @integer>>) {
  // LOWER: scf.yield %{{.*}} : memref<!ada.qual<i32, @integer>>
  // LOWER: } else {
  // LOWER: scf.yield %{{.*}} : memref<!ada.qual<i32, @integer>>
  // LOWER: memref.load %[[SELECTED]][]
  // LLVM-LABEL: define i32 @_ada_region(
  // LLVM: {{(phi ptr|select i1)}}
  // LLVM: load i32
  ada.subp @region(%condition: i1, %v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer> {
    %x = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %v : !ada.qual<i32, @integer>
    %y = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %v : !ada.qual<i32, @integer>
    %r = scf.if %condition -> (!ada.ref<!ada.qual<i32, @integer>>) {
      scf.yield %x : !ada.ref<!ada.qual<i32, @integer>>
    } else {
      scf.yield %y : !ada.ref<!ada.qual<i32, @integer>>
    }
    %value = ada.read %r : !ada.ref<!ada.qual<i32, @integer>> -> !ada.qual<i32, @integer>
    ada.return %value : !ada.qual<i32, @integer>
  }

  // Distinct dynamic instances may coexist: do not hoist the loop allocation.
  // LOWER-LABEL: ada.subp @_ada_loop_instances
  // LOWER: ada.alloca
  // LOWER: cf.br ^bb1
  // LOWER: ^bb1(%[[PREVIOUS:.*]]: memref<!ada.qual<i32, @integer>>):
  // LOWER-NEXT: %[[CURRENT:.*]] = ada.alloca
  // LOWER: memref.store %arg1, %[[CURRENT]][]
  // LOWER: memref.store %{{.*}}, %[[PREVIOUS]][]
  // LOWER: memref.load %[[CURRENT]][]
  // LOWER: cf.cond_br %arg0, ^bb1(%[[CURRENT]] : memref<!ada.qual<i32, @integer>>)
  // LLVM-LABEL: define i32 @_ada_loop_instances(
  // LLVM: alloca i32
  // LLVM: br label
  // LLVM: phi ptr
  // LLVM: alloca i32
  ada.subp @loop_instances(%repeat: i1, %v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer> {
    %initial = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %v : !ada.qual<i32, @integer>
    cf.br ^loop(%initial : !ada.ref<!ada.qual<i32, @integer>>)
  ^loop(%previous: !ada.ref<!ada.qual<i32, @integer>>):
    %current = ada.object : !ada.ref<!ada.qual<i32, @integer>> <- %v : !ada.qual<i32, @integer>
    %one = ada.constant : !ada.qual<i32, @integer> = 1
    ada.write %one, %previous : !ada.qual<i32, @integer> -> !ada.ref<!ada.qual<i32, @integer>>
    %result = ada.read %current : !ada.ref<!ada.qual<i32, @integer>> -> !ada.qual<i32, @integer>
    cf.cond_br %repeat, ^loop(%current : !ada.ref<!ada.qual<i32, @integer>>), ^exit(%result : !ada.qual<i32, @integer>)
  ^exit(%value: !ada.qual<i32, @integer>):
    ada.return %value : !ada.qual<i32, @integer>
  }
}
