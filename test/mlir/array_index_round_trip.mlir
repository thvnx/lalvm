// Round-trip `ada.index`: an array object's location plus a zero-based offset
// map to the element's location, a rank-0 memref at a dynamic offset.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  ada.subp @f(%off: !ada.qual<i32, @standard.integer>) {
    %arr = ada.alloca : memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>
    // CHECK: ada.index %{{.*}}[%{{.*}}] : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>, !ada.qual<i32, @standard.integer>) -> memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>
    %elt = ada.index %arr[%off] : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>, !ada.qual<i32, @standard.integer>) -> memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>
    ada.return
  }
}
