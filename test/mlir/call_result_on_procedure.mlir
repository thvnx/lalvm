// `ada.call` result arity must match the callee kind: a call to a procedure
// cannot carry a result.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.call' op callee 'proc' is a procedure but call carries a result

module {
  ada.subp @proc() {
    ada.return
  }
  ada.subp @bad() -> !ada.qual<i32, @s> {
    %0 = ada.call @proc() : () -> !ada.qual<i32, @s>
    ada.return %0 : !ada.qual<i32, @s>
  }
}
