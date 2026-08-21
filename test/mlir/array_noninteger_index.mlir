// A non-integer index type is rejected by ArrayType's verifier (indices are
// discrete).

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: {{.*}}index type must be an integer

module {
  ada.subp @bad(%a: !ada.array<i32[f32 x 10]>) {
    ada.return
  }
}
