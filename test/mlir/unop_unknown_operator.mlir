// `ada.unop` only accepts the unary operators its enum models (`not` today);
// `UnOp::parse` rejects the rest via `symbolizeAdaUnaryOp`.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: custom op 'ada.unop' unknown unary operator '-'

module {
  ada.subp @bad(%a: i1) -> i1 {
    %0 = ada.unop "-" %a : i1
    ada.return %0 : i1
  }
}
