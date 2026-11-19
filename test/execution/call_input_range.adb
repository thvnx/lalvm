-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=ORDER
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %lalvm -O0 --emit=obj %s -o %t.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2>&1 | %FileCheck %s
-- RUN: %lalvm -O1 --emit=obj %s -o %t.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2>&1 | %FileCheck %s

procedure Call_Input_Range is
   subtype Small is Integer range 1 .. 5;
   X : Integer := 7;

   function Later return Integer is
   begin
      return 1;
   end Later;

   procedure Target (A : in out Small; B : Integer) is
   begin
      A := B;
   end Target;
begin
   -- ORDER-LABEL: func.func @_ada_call_input_range(
   -- ORDER: ada.range_check
   -- ORDER-NEXT: %{{.*}} = call @call_input_range__later(
   -- ORDER-NEXT: %{{.*}} = call @call_input_range__target(
   -- CHECK: raised CONSTRAINT_ERROR : call_input_range.adb:[[#@LINE+1]] range check failed
   Target (X, Later);
end Call_Input_Range;
