// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// RUN: %lalvm --emit=mlir %s | %lalvm -x mlir --emit=mlir - | %FileCheck %s

module {
  // CHECK: ada.type @integer : i32 = #ada.int_info<range 0 to 100> {has_default_value}
  ada.type @integer : i32 = #ada.int_info<range 0 to 100> {has_default_value}
  // CHECK: ada.type @small base @integer : i32 = #ada.int_info<range 1 to 5>
  ada.type @small base @integer : i32 = #ada.int_info<range 1 to 5>
  // CHECK: ada.type @alias base @small : i32
  ada.type @alias base @small : i32
  // CHECK: ada.type @float : f32 = #ada.float_info<digits 6> {has_default_value}
  ada.type @float : f32 = #ada.float_info<digits 6> {has_default_value}
  // CHECK: ada.type @color : i8 = #ada.enum_info<"red" = 7, "green" = 9> {has_default_value}
  ada.type @color : i8 = #ada.enum_info<"red" = 7, "green" = 9> {has_default_value}
}
