// Round-trip for the `ada.binop` `checks<...>` group: it parses and prints
// back, `|`-separated, and only when nonempty. Compiling Ada exercises the
// printer; parsing the textual form here exercises the parse path, including
// the multi-flag `|` separator.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  // CHECK-LABEL: ada.subp @binop_checks
  ada.subp @binop_checks(%a: !ada.qual<i32, @integer>,
                         %b: !ada.qual<i32, @integer>)
      -> !ada.qual<i32, @integer> {
    // CHECK: ada.binop "+" %arg0, %arg1 checks<overflow> : !ada.qual<i32, @integer>
    %0 = ada.binop "+" %a, %b checks<overflow> : !ada.qual<i32, @integer>

    // CHECK: ada.binop "*" %arg0, %arg1 checks<overflow|division> : !ada.qual<i32, @integer>
    %1 = ada.binop "*" %a, %b checks<overflow|division> : !ada.qual<i32, @integer>

    // No group when empty.
    // CHECK: ada.binop "-" %arg0, %arg1 : !ada.qual<i32, @integer>
    %2 = ada.binop "-" %a, %b : !ada.qual<i32, @integer>

    ada.return %0 : !ada.qual<i32, @integer>
  }
}
