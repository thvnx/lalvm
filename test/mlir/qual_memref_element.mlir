// An !ada.qual is a valid memref element type (it implements
// MemRefElementTypeInterface).

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

// CHECK: ada.alloca : memref<!ada.qual<i32, @integer>>

module {
  ada.subp @f() {
    %0 = ada.alloca : memref<!ada.qual<i32, @integer>>
    ada.return
  }
}
