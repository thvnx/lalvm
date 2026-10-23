// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// `ada.range_check` lowers to a signedness-aware compare (here signed, the
// default for a non-modular subtype) of the value against the two bounds read
// from the descriptor, and a conditional branch to a raise block that calls
// the GNAT runtime `__gnat_rcheck_CE_Range_Check(file, line)` and is
// unreachable. The machine value flows through unchanged, although the result
// is qualified by the subtype and the value by its base. Fed as MLIR. The
// bound `!ada.qual`s erase to their machine type on lowering.

// RUN: %lalvm --emit=llvm %s | %FileCheck %s

module {
  // CHECK-LABEL: define i32 @_ada_check
  ada.subp @check(%v: !ada.qual<i32, @integer>, %lo: !ada.qual<i32, @integer>,
                  %hi: !ada.qual<i32, @integer>) -> !ada.qual<i32, @positive> {
    %r = ada.range %lo, %hi : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    // CHECK:      %[[LO:.*]] = extractvalue { i32, i32 } %{{.*}}, 0
    // CHECK:      %[[HI:.*]] = extractvalue { i32, i32 } %{{.*}}, 1
    // CHECK:      icmp slt i32 %0, %[[LO]]
    // CHECK:      icmp sgt i32 %0, %[[HI]]
    // CHECK:      or i1
    // CHECK:      br i1
    // CHECK:      call void @__gnat_rcheck_CE_Range_Check(ptr {{.*}}, i32 {{.*}})
    // CHECK:      unreachable
    // CHECK:      ret i32 %0
    %c = ada.range_check %v, %r : !ada.qual<i32, @integer> to !ada.qual<i32, @positive>
    ada.return %c : !ada.qual<i32, @positive>
  }
}
