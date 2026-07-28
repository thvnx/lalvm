// `ada.cmp` operands must be integer or float machine types.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.cmp' op unsupported operand type 'index'; expected integer or float

module {
  ada.subp @bad(%a: !ada.qual<index, @s>, %b: !ada.qual<index, @s>) -> !ada.qual<i1, @b> {
    %0 = ada.cmp "=" %a, %b : (!ada.qual<index, @s>, !ada.qual<index, @s>) -> !ada.qual<i1, @b>
    ada.return %0 : !ada.qual<i1, @b>
  }
}
