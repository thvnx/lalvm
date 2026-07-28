// `ada.call` result arity must match the callee kind: a call to a function
// must carry a result.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.call' op callee 'func' is a function but call carries no result

module {
  ada.subp @func() -> !ada.qual<i32, @s> {
    %0 = ada.constant : !ada.qual<i32, @s> = 1
    ada.return %0 : !ada.qual<i32, @s>
  }
  ada.subp @bad() {
    ada.call @func() : () -> ()
    ada.return
  }
}
