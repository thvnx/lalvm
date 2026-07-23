// Widening `ada.coerce`: `sext` for a signed source, `zext` for a modular
// one (read from the type symbol's `mod`), `fpext` for floats. Fed as MLIR:
// no Ada path emits a widening coerce while explicit type conversions are
// unsupported.

// RUN: %lalvm --emit=llvm %s | %FileCheck %s

module {
  ada.type @short_integer : i16 = #ada.int_info<range -32768 to 32767>
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.type @byte : i8 = #ada.int_info<mod 256>
  ada.type @word : i16 = #ada.int_info<mod 65536>
  ada.type @float : f32 = #ada.float_info<digits 6>
  ada.type @long_float : f64 = #ada.float_info<digits 15>

  // CHECK-LABEL: define i32 @_ada_widen_signed
  ada.subp @widen_signed(%x: !ada.qual<i16, @short_integer>) -> !ada.qual<i32, @integer> {
    // CHECK: sext i16 %0 to i32
    %r = ada.coerce %x : <i16, @short_integer> to <i32, @integer>
    ada.return %r : !ada.qual<i32, @integer>
  }

  // CHECK-LABEL: define i16 @_ada_widen_modular
  ada.subp @widen_modular(%x: !ada.qual<i8, @byte>) -> !ada.qual<i16, @word> {
    // CHECK: zext i8 %0 to i16
    %r = ada.coerce %x : <i8, @byte> to <i16, @word>
    ada.return %r : !ada.qual<i16, @word>
  }

  // CHECK-LABEL: define double @_ada_widen_float
  ada.subp @widen_float(%x: !ada.qual<f32, @float>) -> !ada.qual<f64, @long_float> {
    // CHECK: fpext float %0 to double
    %r = ada.coerce %x : <f32, @float> to <f64, @long_float>
    ada.return %r : !ada.qual<f64, @long_float>
  }
}
