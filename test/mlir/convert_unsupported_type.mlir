// `ada.convert` converts numeric values only.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.convert' op unsupported target type: 'index'

module {
  ada.subp @bad(%x: !ada.qual<i32, @s>) -> !ada.qual<index, @t> {
    %0 = ada.convert explicit %x : !ada.qual<i32, @s> to !ada.qual<index, @t>
    ada.return %0 : !ada.qual<index, @t>
  }
}
