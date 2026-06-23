// `ada.range` bounds are Ada-qualified scalar values: the verifier rejects a
// range whose bound machine type is neither an integer nor a float (here,
// `index`).

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.range' op range bound type must be integer or float

module {
  ada.subp @bad(%lo: !ada.qual<index, @s>, %hi: !ada.qual<index, @s>) {
    %r = ada.range %lo, %hi : !ada.qual<index, @s> -> !ada.range<index, @s>
    ada.return
  }
}
