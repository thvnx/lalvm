// An `ada.coerce` between identical types is a no-op that must not exist.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.coerce' op input and result types are identical; use the value directly

module {
  ada.subp @bad(%x: !ada.qual<i32, @s>) -> !ada.qual<i32, @s> {
    %0 = ada.coerce %x : !ada.qual<i32, @s> to !ada.qual<i32, @s>
    ada.return %0 : !ada.qual<i32, @s>
  }
}
