-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- An enumeration whose representation clause gives a negative code (RM 13.4)
-- orders by signed comparison of its codes.

-- CHECK-LABEL: define internal i1 @enum_order_signed__cmp(
-- CHECK-DAG: icmp slt i11
-- CHECK-DAG: icmp sge i11
-- CHECK-DAG: icmp sle i11
-- CHECK-DAG: icmp sgt i11

-- CHECK-LABEL: define internal i1 @enum_order_signed__cmp_sub(
-- CHECK-DAG: icmp slt i11
-- CHECK-DAG: icmp sge i11

function Enum_Order_Signed return Boolean is
   type Color is (Red, Green, Blue);
   for Color use (Red => -1000, Green => 42, Blue => 99);
   subtype Warm is Color range Red .. Green;
   function Cmp (A, B : Color) return Boolean is
   begin
      return A < B and B >= A and A <= B and B > A;
   end Cmp;
   function Cmp_Sub (A, B : Warm) return Boolean is
   begin
      return A < B and B >= A;
   end Cmp_Sub;
begin
   return Cmp (Red, Green) and Cmp_Sub (Red, Green);
end Enum_Order_Signed;
