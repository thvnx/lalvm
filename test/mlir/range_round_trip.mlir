// Round-trip coverage for the `!ada.range` type and the `ada.range` op. The
// bounds are `ada.qual` values of the subtype's base type; the printed form
// spells the bound `!ada.qual` then the `!ada.range` result, so parsing the
// textual form back in exercises that path.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  // CHECK-LABEL: ada.subp @ranges
  ada.subp @ranges(%n: !ada.qual<i32, @integer>) {
    // CHECK: %[[LO:.*]] = ada.constant : !ada.qual<i32, @integer> = 1
    %lo = ada.constant : !ada.qual<i32, @integer> = 1
    // CHECK: %[[HI:.*]] = ada.constant : !ada.qual<i32, @integer> = 100
    %hi = ada.constant : !ada.qual<i32, @integer> = 100

    // Static bounds: constant operands.
    // CHECK: ada.range %[[LO]], %[[HI]] : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    %r = ada.range %lo, %hi : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>

    // Dynamic upper bound: a runtime SSA value.
    // CHECK: ada.range %[[LO]], %arg0 : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    %rd = ada.range %lo, %n : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>

    // Float bounds.
    // CHECK: ada.range %{{.*}}, %{{.*}} : !ada.qual<f32, @float> -> !ada.range<f32, @float>
    %flo = ada.constant : !ada.qual<f32, @float> = 0.000000e+00
    %fhi = ada.constant : !ada.qual<f32, @float> = 1.000000e+00
    %fr = ada.range %flo, %fhi : !ada.qual<f32, @float> -> !ada.range<f32, @float>

    ada.return
  }
}
