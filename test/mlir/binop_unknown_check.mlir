// The `checks<...>` group only accepts the `AdaChecks` bit-enum flags
// (`overflow`, `division`); `parseChecks` rejects anything else.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: custom op 'ada.binop' unknown check 'bogus'

module {
  ada.subp @bad(%a: i32, %b: i32) -> i32 {
    %0 = ada.binop "+" %a, %b checks<bogus> : i32
    ada.return %0 : i32
  }
}
