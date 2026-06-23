// `ada.attr` lowers to an `extractvalue` of the lowered range descriptor
// (`!llvm.struct<(i32, i32)>`): `'First` reads field 0, `'Last` field 1. The
// checked value's subtype symbol carries no run-time data, so an undefined
// `@positive` is fine here. The bound `!ada.qual`s erase to their machine type
// on lowering. Fed as MLIR (no Ada emitter yet).

// RUN: %lalvm --emit=llvm %s | %FileCheck %s

module {
  // CHECK-LABEL: define i32 @_ada_first_attr
  ada.subp @first_attr(%lo: !ada.qual<i32, @integer>, %hi: !ada.qual<i32, @integer>)
      -> !ada.qual<i32, @positive> {
    %r = ada.range %lo, %hi : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    // CHECK: %[[F:.*]] = extractvalue { i32, i32 } %{{.*}}, 0
    // CHECK: ret i32 %[[F]]
    %f = ada.attr "first", %r : !ada.qual<i32, @positive>
    ada.return %f : !ada.qual<i32, @positive>
  }

  // CHECK-LABEL: define i32 @_ada_last_attr
  ada.subp @last_attr(%lo: !ada.qual<i32, @integer>, %hi: !ada.qual<i32, @integer>)
      -> !ada.qual<i32, @positive> {
    %r = ada.range %lo, %hi : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    // CHECK: %[[L:.*]] = extractvalue { i32, i32 } %{{.*}}, 1
    // CHECK: ret i32 %[[L]]
    %l = ada.attr "last", %r : !ada.qual<i32, @positive>
    ada.return %l : !ada.qual<i32, @positive>
  }
}
