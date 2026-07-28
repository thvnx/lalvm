// `ada.return` arity must match the enclosing subprogram's results.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: 'ada.return' op does not return the same number of values (0) as the enclosing function (1)

module {
  ada.subp @bad() -> !ada.qual<i32, @s> {
    ada.return
  }
}
