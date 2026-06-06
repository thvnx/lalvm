// `ada.constant` must carry an Ada type identity: its result type is an
// `!ada.qual`. A bare builtin type is rejected by `ConstantOp::parse`.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: custom op 'ada.constant' expected !ada.qual result type

module {
  ada.subp @bad() {
    %0 = ada.constant : i32 = 42
    ada.return
  }
}
