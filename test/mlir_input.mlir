// RUN: %lalvm --emit=llvm %s | %FileCheck %s

// CHECK-LABEL: define i32 @test_from_mlir(
// CHECK:         ret i32 42

module {
  ada.func @test_from_mlir() -> i32 {
    %0 = arith.constant 42 : i32
    ada.return %0 : i32
  }
}
