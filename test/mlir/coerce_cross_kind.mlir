// `ada.coerce` converts within a kind (int-to-int, float-to-float); a
// cross-kind int-to-float conversion is rejected.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.coerce' op unsupported conversion: 'i32' to 'f32'

module {
  ada.subp @bad(%x: !ada.qual<i32, @s>) -> !ada.qual<f32, @t> {
    %0 = ada.coerce %x : !ada.qual<i32, @s> to !ada.qual<f32, @t>
    ada.return %0 : !ada.qual<f32, @t>
  }
}
