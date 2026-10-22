// Implicit `ada.convert` stays within one numeric class, i.e. integer to float is rejected.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.convert' op implicit conversion between integer and float: 'i32' to 'f32'

module {
  ada.subp @bad(%x: !ada.qual<i32, @s>) -> !ada.qual<f32, @t> {
    %0 = ada.convert implicit %x : !ada.qual<i32, @s> to !ada.qual<f32, @t>
    ada.return %0 : !ada.qual<f32, @t>
  }
}
