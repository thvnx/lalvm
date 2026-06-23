// Round-trip coverage for the `ada.attr` op (`'First`/`'Last`, RM 3.5). It
// reads a bound from a subtype's `!ada.range` descriptor and yields it as the
// subtype's `!ada.qual`. The op prints only its result type; the range operand
// type is recovered from the result subtype on parse (the `TypesMatchWith`
// inference), so parsing the textual form back exercises that path. Both a
// static range (constant bounds) and a dynamic one (a runtime bound) are
// covered. The `ada.range` bounds are `ada.qual` values of the base type. Fed
// as MLIR (no Ada emitter yet).

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  // CHECK-LABEL: ada.subp @attrs
  ada.subp @attrs(%n: !ada.qual<i32, @integer>) -> !ada.qual<i32, @positive> {
    %lo = ada.constant : !ada.qual<i32, @integer> = 1
    %hi = ada.constant : !ada.qual<i32, @integer> = 100

    // Static range: 'First and 'Last read its constant bounds.
    %r = ada.range %lo, %hi : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    // CHECK: %[[F:.*]] = ada.attr "first", %{{.*}} : !ada.qual<i32, @positive>
    %first = ada.attr "first", %r : !ada.qual<i32, @positive>
    // CHECK: ada.attr "last", %{{.*}} : !ada.qual<i32, @positive>
    %last = ada.attr "last", %r : !ada.qual<i32, @positive>

    // Dynamic range: the upper bound is a runtime value; 'Last reads it at run
    // time once lowered.
    %rd = ada.range %lo, %n : !ada.qual<i32, @integer> -> !ada.range<i32, @positive>
    // CHECK: ada.attr "last", %{{.*}} : !ada.qual<i32, @positive>
    %dlast = ada.attr "last", %rd : !ada.qual<i32, @positive>

    ada.return %first : !ada.qual<i32, @positive>
  }
}
