// A scalar object cannot be indexed.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: {{.*}}array object must have an ada.array value type

module {
  ada.subp @f(%off: !ada.qual<i32, @standard.integer>) {
    %arr = ada.object : !ada.ref<!ada.qual<i32, @arr>>
    %elt = ada.index %arr[%off] : (!ada.ref<!ada.qual<i32, @arr>>, !ada.qual<i32, @standard.integer>) -> memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>
    ada.return
  }
}
