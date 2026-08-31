// The array operand must be a memref of a qual-wrapped !ada.array. Here it is a
// scalar cell (a qual of i32), so the verifier rejects it.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: {{.*}}memref of ada.qual<!ada.array>

module {
  ada.subp @f(%off: !ada.qual<i32, @standard.integer>) {
    %arr = ada.alloca : memref<!ada.qual<i32, @arr>>
    %elt = ada.index %arr[%off] : (memref<!ada.qual<i32, @arr>>, !ada.qual<i32, @standard.integer>) -> memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>
    ada.return
  }
}
