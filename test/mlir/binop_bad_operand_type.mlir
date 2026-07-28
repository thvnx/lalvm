// `ada.binop` operands must be integer or float machine types (possibly
// wrapped in `!ada.qual`); `index` is neither.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.binop' op unsupported operand type 'index'; expected integer or float

module {
  ada.subp @bad(%a: !ada.qual<index, @s>, %b: !ada.qual<index, @s>) -> !ada.qual<index, @s> {
    %0 = ada.binop "+" %a, %b : !ada.qual<index, @s>
    ada.return %0 : !ada.qual<index, @s>
  }
}
