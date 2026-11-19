-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- XFAIL: *
-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- A is read on entry and is never assigned.
-- CHECK: warning: {{.*}}is read but never assigned

procedure Param_Out_Never_Assigned is
   procedure P (A, B : out Integer) is
      X : Integer := A;
   begin
      B := X;
   end P;
begin
   null;
end Param_Out_Never_Assigned;
