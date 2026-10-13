-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- Check that an up-level reference in an if expression uses the lifted parameter.

-- CHECK-LABEL: define i1 @_ada_capture_nested_region(i32 %0)
-- CHECK:         %[[LIMIT:.*]] = alloca i32
-- CHECK:         call i1 @capture_nested_region__in_if_expr(i32 1, ptr %[[LIMIT]])

-- CHECK-LABEL: define internal i1 @capture_nested_region__in_if_expr(i32 %0, ptr %1)
-- CHECK:         br i1 %{{.*}}, label %[[THEN:[0-9]+]],
-- CHECK:       {{^}}[[THEN]]:
-- CHECK-NEXT:    load i32, ptr %1

function Capture_Nested_Region (A : Integer) return Boolean is
   Limit : Integer := A;

   function In_If_Expr (X : Integer) return Boolean is
   begin
      return (if X > 0 then X < Limit else False);
   end In_If_Expr;
begin
   return In_If_Expr (1);
end Capture_Nested_Region;
