// The result element must be the array's component type. Here the array
// component is i32 but the result element is f32, so the verifier rejects it.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: {{.*}}does not match array component

module {
  ada.subp @f(%off: !ada.qual<i32, @standard.integer>) {
    %arr = ada.object : !ada.ref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>
    %elt = ada.index %arr[%off] : (!ada.ref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>, !ada.qual<i32, @standard.integer>) -> memref<!ada.qual<f32, @standard.float>, strided<[], offset: ?>>
    ada.return
  }
}
