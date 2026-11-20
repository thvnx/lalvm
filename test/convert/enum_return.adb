-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s

-- CHECK-LABEL: func.func private @enum_return__narrow(
-- CHECK: %[[LO:.*]] = ada.constant : !ada.qual<i8, @enum_return.color> = 42
-- CHECK-NEXT: %[[HI:.*]] = ada.constant : !ada.qual<i8, @enum_return.color> = 99
-- CHECK-NEXT: %[[RANGE:.*]] = ada.range %[[LO]], %[[HI]]
-- CHECK-NEXT: ada.range_check %x, %[[RANGE]] : !ada.qual<i8, @enum_return.color> to !ada.qual<i8, @enum_return.tail>

procedure Enum_Return is
   type Color is (Red, Green, Blue);
   for Color use (Red => -10, Green => 42, Blue => 99);
   subtype Tail is Color range Green .. Blue;

   function Narrow (X : Color) return Tail is
   begin
      return X;
   end Narrow;
begin
   null;
end Enum_Return;
