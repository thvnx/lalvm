// `ada.alloca` allocates Ada-typed storage: its memref element type must be
// an `!ada.qual`, so the value keeps its Ada type identity through loads.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.alloca' op result element type must be ada.qual

module {
  ada.subp @bad() {
    %0 = ada.alloca : memref<i32>
    ada.return
  }
}
