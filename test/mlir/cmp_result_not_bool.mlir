// A relational operator yields Boolean (RM 4.5.2): the `ada.cmp` result's
// machine type must be `i1`.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.cmp' op result must be Boolean (i1), got 'i32'

module {
  ada.subp @bad(%a: !ada.qual<i32, @s>, %b: !ada.qual<i32, @s>) -> !ada.qual<i32, @s> {
    %0 = ada.cmp "=" %a, %b : (!ada.qual<i32, @s>, !ada.qual<i32, @s>) -> !ada.qual<i32, @s>
    ada.return %0 : !ada.qual<i32, @s>
  }
}
