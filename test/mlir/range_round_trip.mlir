// Round-trip coverage for the `!ada.range` type and the `ada.range` op.
// `ada.range` prints only its result type; the bound operand types are
// recovered from the range's `boundType` on parse (the `TypesMatchWith`
// inference), so parsing the textual form back in exercises that path.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  // CHECK-LABEL: ada.subp @ranges
  ada.subp @ranges(%n: i32) {
    // CHECK: %[[LO:.*]] = arith.constant 1 : i32
    %lo = arith.constant 1 : i32
    // CHECK: %[[HI:.*]] = arith.constant 100 : i32
    %hi = arith.constant 100 : i32

    // Static bounds: constant operands.
    // CHECK: ada.range %[[LO]], %[[HI]] : !ada.range<i32, @positive>
    %r = ada.range %lo, %hi : !ada.range<i32, @positive>

    // Dynamic upper bound: a runtime SSA value.
    // CHECK: ada.range %[[LO]], %arg0 : !ada.range<i32, @positive>
    %rd = ada.range %lo, %n : !ada.range<i32, @positive>

    // Float bounds.
    // CHECK: ada.range %{{.*}}, %{{.*}} : !ada.range<f32, @float>
    %flo = arith.constant 0.000000e+00 : f32
    %fhi = arith.constant 1.000000e+00 : f32
    %fr = ada.range %flo, %fhi : !ada.range<f32, @float>

    ada.return
  }
}
