// Round-trip coverage for the `ada.mode` parameter-mode arg attr (@rm{6-1}).
// `in` is normally left implicit by MLIRGen, but the enum and its parse/print
// cover all three modes, so all three are exercised here. The attr is
// independent of the argument type, so bare `!ada.qual` args suffice (the
// realistic by-reference `memref` form is covered by the .adb signature tests).

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  ada.type @integer : i32 = #ada.int_info<range -2147483648 to 2147483647>

  // CHECK-LABEL: ada.subp @modes
  // CHECK-SAME: %arg0: !ada.qual<i32, @integer> {ada.mode = #ada<mode in>}
  // CHECK-SAME: %arg1: !ada.qual<i32, @integer> {ada.mode = #ada<mode out>}
  // CHECK-SAME: %arg2: !ada.qual<i32, @integer> {ada.mode = #ada<mode in_out>}
  ada.subp @modes(%arg0: !ada.qual<i32, @integer> {ada.mode = #ada<mode in>},
                  %arg1: !ada.qual<i32, @integer> {ada.mode = #ada<mode out>},
                  %arg2: !ada.qual<i32, @integer> {ada.mode = #ada<mode in_out>}) {
    ada.return
  }
}
