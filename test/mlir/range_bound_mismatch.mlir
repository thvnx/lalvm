// The bounds' machine type must equal the range's bound type
// (`range_invalid_bound_type.mlir` hits the earlier non-scalar check).

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.range' op bound machine type 'i16' does not match range bound type 'i32'

module {
  ada.subp @bad(%lo: !ada.qual<i16, @s>, %hi: !ada.qual<i16, @s>) {
    %r = ada.range %lo, %hi : !ada.qual<i16, @s> -> !ada.range<i32, @s>
    ada.return
  }
}
