// An !ada.qual is a valid memref element type (it implements
// MemRefElementTypeInterface).

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

// CHECK: memref.alloca() : memref<!ada.qual<i32, @integer>>

module {
  func.func @f() {
    %0 = memref.alloca() : memref<!ada.qual<i32, @integer>>
    return
  }
}
