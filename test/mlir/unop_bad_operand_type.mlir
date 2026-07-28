// `ada.unop` (`not`) operates on integers (Boolean or modular); a float
// operand is rejected.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.unop' op unsupported operand type 'f32'; expected integer (Boolean)

module {
  ada.subp @bad(%a: !ada.qual<f32, @s>) -> !ada.qual<f32, @s> {
    %0 = ada.unop "not" %a : !ada.qual<f32, @s>
    ada.return %0 : !ada.qual<f32, @s>
  }
}
