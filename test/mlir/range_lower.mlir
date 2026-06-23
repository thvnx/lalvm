// `ada.range` lowers to a `{ T, T }` LLVM struct: `undef` + one `insertvalue`
// per bound. Fed as MLIR (no Ada emitter yet); returned so it survives DCE,
// with runtime-parameter bounds so the `insertvalue`s are not constant-folded.
// The bound `!ada.qual`s erase to their machine type on lowering.

// RUN: %lalvm --emit=llvm %s | %FileCheck %s

module {
  // The result type also exercises the `!ada.range` -> `{ i32, i32 }` conversion.
  // CHECK-LABEL: define { i32, i32 } @_ada_make_range
  ada.subp @make_range(%lo: !ada.qual<i32, @integer>, %hi: !ada.qual<i32, @integer>)
      -> !ada.range<i32, @positive> {
    // CHECK: %[[R0:.*]] = insertvalue { i32, i32 } undef, i32 %0, 0
    // CHECK: %[[R1:.*]] = insertvalue { i32, i32 } %[[R0]], i32 %1, 1
    %r = ada.range %lo, %hi : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    // CHECK: ret { i32, i32 } %[[R1]]
    ada.return %r : !ada.range<i32, @positive>
  }
}
