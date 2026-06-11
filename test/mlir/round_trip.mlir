// Round-trip coverage for the Ada dialect ops with a custom assembly format
// (`ada.type`, `ada.constant`, `ada.binop`, `ada.cmp`). Compiling Ada source
// only exercises their printers and lowering; parsing the textual form back
// in exercises the `parse()` paths. The functional form of `ada.binop` is
// normalized to the short form on print, so it is checked separately.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  // CHECK: ada.type @color : i8 = #ada.enum_info<"red" = 0, "green" = 1, "blue" = 2>
  ada.type @color : i8 = #ada.enum_info<"red" = 0, "green" = 1, "blue" = 2>

  // CHECK: ada.type @integer : i32 = #ada.int_info
  ada.type @integer : i32 = #ada.int_info

  // CHECK: ada.type @byte : i32 = #ada.int_info<mod 256>
  ada.type @byte : i32 = #ada.int_info<mod 256>

  // CHECK: ada.type @float : f32 = #ada.float_info
  ada.type @float : f32 = #ada.float_info

  // CHECK-LABEL: ada.subp @ops
  ada.subp @ops(%a: !ada.qual<i32, @integer>, %b: !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer> {
    // CHECK: %[[SUM:.*]] = ada.binop "+" %arg0, %arg1 : !ada.qual<i32, @integer>
    %0 = ada.binop "+" %a, %b : !ada.qual<i32, @integer>

    // The functional form parses, but prints back in the short form.
    // CHECK: ada.binop "*" %arg0, %arg1 : !ada.qual<i32, @integer>
    %1 = ada.binop "*" %a, %b : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer>

    // CHECK: ada.cmp "=" %arg0, %arg1 : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i1, @boolean>
    %2 = ada.cmp "=" %a, %b : (!ada.qual<i32, @integer>, !ada.qual<i32, @integer>) -> !ada.qual<i1, @boolean>

    // CHECK: ada.constant : !ada.qual<i32, @integer> = 42
    %3 = ada.constant : !ada.qual<i32, @integer> = 42

    ada.return %0 : !ada.qual<i32, @integer>
  }
}
