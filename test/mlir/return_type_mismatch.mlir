// The `ada.return` operand type must match the subprogram's result type.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.return' op type of return operand ('!ada.qual<i16, @t>') doesn't match function result type ('!ada.qual<i32, @s>')

module {
  ada.subp @bad(%x: !ada.qual<i16, @t>) -> !ada.qual<i32, @s> {
    ada.return %x : !ada.qual<i16, @t>
  }
}
