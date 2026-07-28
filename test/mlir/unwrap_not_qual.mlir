// `ada.unwrap` exposes the machine value under an `!ada.qual`; a bare
// builtin operand has nothing to unwrap.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.unwrap' op operand must be an ada.qual type, got 'i32'

module {
  ada.subp @bad(%a: i32) {
    %0 = ada.unwrap %a : i32 to i32
    ada.return
  }
}
