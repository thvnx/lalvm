// The index must be an integer-machine qual. Here it is an f32 qual, so the
// verifier rejects it.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: {{.*}}index type must be an integer

module {
  ada.subp @f(%off: !ada.qual<f32, @standard.float>) {
    %arr = ada.object : !ada.ref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>
    %elt = ada.index %arr[%off] : (!ada.ref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>, !ada.qual<f32, @standard.float>) -> memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>
    ada.return
  }
}
