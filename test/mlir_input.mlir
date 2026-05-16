// RUN: %lalvm --emit=llvm %s | %FileCheck %s

// CHECK-LABEL: define i32 @_ada_test_from_mlir(
// CHECK:         ret i32 42

module {
  ada.subp @test_from_mlir() -> i32 {
    %0 = arith.constant 42 : i32
    ada.return %0 : i32
  }
}
