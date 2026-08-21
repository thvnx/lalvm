// A negative, non-dynamic extent is rejected by ArrayType's verifier.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: {{.*}}extent must be non-negative or dynamic

module {
  ada.subp @bad(%a: !ada.array<i32[i32 x -5]>) {
    ada.return
  }
}
