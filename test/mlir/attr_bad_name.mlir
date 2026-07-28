// `ada.attr` models the bound-reading attributes only ('First/'Last).

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.attr' op unsupported attribute 'length'; expected "first" or "last"

module {
  ada.subp @bad(%lo: !ada.qual<i32, @s>, %hi: !ada.qual<i32, @s>) -> !ada.qual<i32, @s> {
    %r = ada.range %lo, %hi : !ada.qual<i32, @s> -> !ada.range<i32, @s>
    %0 = ada.attr "length", %r : !ada.qual<i32, @s>
    ada.return %0 : !ada.qual<i32, @s>
  }
}
