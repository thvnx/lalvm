// `ada.binop` only accepts the predefined arithmetic operators (+, -, *, /).
// A relational operator such as "=" belongs to `ada.cmp`, so `BinOp::parse`
// rejects it via `symbolizeAdaBinaryOp`.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: custom op 'ada.binop' unknown binary operator '='

module {
  ada.subp @bad(%a: i32, %b: i32) -> i32 {
    %0 = ada.binop "=" %a, %b : i32
    ada.return %0 : i32
  }
}
