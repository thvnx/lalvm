-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s --implicit-check-not=warning --implicit-check-not=ada.index_check
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %t; test $? -eq 42

-- True reads as 1, needs no index check and selects the second component.
-- CHECK: ada.index

function Exec_Bool_Index return Integer is
   type By_Bool is array (False .. True) of Integer;
   B : By_Bool;
   F : Boolean := False;
   T : Boolean := True;
begin
   B (False) := 40;
   B (True) := 2;
   return B (F) + B (T);
end Exec_Bool_Index;
