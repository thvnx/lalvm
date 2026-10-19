-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- Range check of an `out` parameter of a dynamic subtype in a nested subprogram.

procedure Exec_Out_Param_Dynamic_Range is
   procedure Run (N : Integer) is
      subtype S is Integer range 1 .. N;
      procedure Set (X : out S; Value : Integer) is
      begin
         -- CHECK: raised CONSTRAINT_ERROR : exec_out_param_dynamic_range.adb:[[#@LINE+1]] range check failed
         X := Value;
      end Set;
      V : S;
   begin
      Set (V, 10);
      Set (V, 11);
   end Run;
begin
   Run (10);
end Exec_Out_Param_Dynamic_Range;
