// A memref cell of a qual-wrapped !ada.array parses from text and round-trips.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

// CHECK: ada.alloca : memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>

module {
  ada.subp @f() {
    %0 = ada.alloca : memref<!ada.qual<!ada.array<i32[i32 x 10]>, @arr>>
    ada.return
  }
}
