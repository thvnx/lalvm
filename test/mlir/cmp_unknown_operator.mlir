// `ada.cmp` only accepts the relational operators it lowers (= and /=). Any
// other symbol is rejected by `CmpOp::parse` via `symbolizeAdaRelationalOp`.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: custom op 'ada.cmp' unknown relational operator '*'

module {
  ada.subp @bad(%a: i32, %b: i32) -> i1 {
    %0 = ada.cmp "*" %a, %b : (i32, i32) -> i1
    ada.return %0 : i1
  }
}
