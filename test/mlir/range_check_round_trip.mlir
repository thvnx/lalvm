// Round-trip coverage for the `ada.range_check` op. It prints only its result
// `!ada.qual` type; the value operand (same type) and the `!ada.range` operand
// are recovered from it on parse (the `TypesMatchWith` inference), so parsing
// the textual form back in exercises that path. The `ada.range` bounds are
// `ada.qual` values of the base type, printed ahead of the `!ada.range`.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  // CHECK-LABEL: ada.subp @checks
  ada.subp @checks(%v: !ada.qual<i32, @positive>, %n: !ada.qual<i32, @integer>) {
    // CHECK: %[[LO:.*]] = ada.constant : !ada.qual<i32, @integer> = 1
    %lo = ada.constant : !ada.qual<i32, @integer> = 1

    // Static bounds.
    // CHECK: %[[R:.*]] = ada.range %[[LO]], %{{.*}} : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    %hi = ada.constant : !ada.qual<i32, @integer> = 100
    %r = ada.range %lo, %hi : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    // CHECK: ada.range_check %arg0, %[[R]] : !ada.qual<i32, @positive>
    %c = ada.range_check %v, %r : !ada.qual<i32, @positive>

    // Dynamic upper bound: the check is the same op, the range is runtime.
    // CHECK: %[[RD:.*]] = ada.range %[[LO]], %arg1 : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    %rd = ada.range %lo, %n : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    // CHECK: ada.range_check %arg0, %[[RD]] : !ada.qual<i32, @positive>
    %cd = ada.range_check %v, %rd : !ada.qual<i32, @positive>

    ada.return
  }
}
