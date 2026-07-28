// The `ada.constant` value must be an `IntegerAttr` or `FloatAttr`. The
// parser types any attribute it accepts to the result's machine type, so a
// string arrives as a `StringAttr` typed `i32` and only the kind check
// catches it.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.constant' op value must be an integer or float attribute

module {
  ada.subp @bad() {
    %0 = ada.constant : !ada.qual<i32, @s> = "boom"
    ada.return
  }
}
