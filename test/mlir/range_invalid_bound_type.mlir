// `ada.range` bounds are scalar machine values: the verifier rejects a
// `boundType` that is neither an integer nor a float (here, `index`).

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.range' op range bound type must be integer or float

module {
  ada.subp @bad() {
    %lo = arith.constant 0 : index
    %hi = arith.constant 9 : index
    %r = ada.range %lo, %hi : !ada.range<index, @s>
    ada.return
  }
}
