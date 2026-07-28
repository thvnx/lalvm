// `ada.unwrap` is value-preserving: the result type must be exactly the
// operand's underlying machine type.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.unwrap' op result type 'i64' must be the operand's underlying type 'i32'

module {
  ada.subp @bad(%a: !ada.qual<i32, @s>) {
    %0 = ada.unwrap %a : !ada.qual<i32, @s> to i64
    ada.return
  }
}
