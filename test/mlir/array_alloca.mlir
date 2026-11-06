// A memref cell of a qual-wrapped !ada.array parses from text and round-trips.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

// CHECK: memref.alloca() : memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>

module {
  func.func @f() {
    %0 = memref.alloca() : memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>
    return
  }
}
