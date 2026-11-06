// Round-trip `ada.index`: an array object and an Ada index select a component
// reference, preserving the nominal component type.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  ada.subp @f(%index: !ada.qual<i32, @standard.integer>) {
    %arr = ada.alloca : memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>
    // CHECK: ada.index %{{.*}}[%{{.*}}] : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>, !ada.qual<i32, @standard.integer>) -> !ada.ref<!ada.qual<i32, @standard.integer>>
    %elt = ada.index %arr[%index] : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>, !ada.qual<i32, @standard.integer>) -> !ada.ref<!ada.qual<i32, @standard.integer>>
    ada.return
  }
}
