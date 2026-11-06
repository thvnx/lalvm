// Round-trip `ada.index`: an array object and an Ada index select a component
// reference, preserving the nominal component type.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s
// RUN: %lalvm --emit=mlir %s | %lalvm -x mlir --emit=mlir - | %FileCheck %s

module {
  ada.subp @f(%index: !ada.qual<i32, @standard.integer>) {
    %arr = ada.alloca : memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>
    // CHECK: ada.index %{{.*}}[%{{.*}}] : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>, !ada.qual<i32, @standard.integer>) -> !ada.ref<!ada.qual<i32, @standard.integer>>
    %elt = ada.index %arr[%index] : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>, !ada.qual<i32, @standard.integer>) -> !ada.ref<!ada.qual<i32, @standard.integer>>
    ada.return
  }

  // CHECK-LABEL: ada.subp @normalized
  ada.subp @normalized(%index: !ada.qual<i32, @standard.integer>, %offset: i64) {
    %arr = ada.alloca : memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>
    // CHECK: ada.index %{{.*}}[%arg0] offset %arg1 : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>, !ada.qual<i32, @standard.integer>, i64) -> !ada.ref<!ada.qual<i32, @standard.integer>>
    %elt = ada.index %arr[%index] offset %offset : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>, !ada.qual<i32, @standard.integer>, i64) -> !ada.ref<!ada.qual<i32, @standard.integer>>
    ada.return
  }
}
