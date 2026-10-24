-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- Narrowing conversion of a modular value: 65_535 reads as unsigned, not as
-- the -1 of its bit pattern, which lies in Small.

procedure Conversion_Modular_Narrowing is
   type Word is mod 65_536;
   type Small is range -100 .. 100;
   W : Word := 65_535;
   S : Small;
begin
   -- CHECK: raised CONSTRAINT_ERROR : conversion_modular_narrowing.adb:[[#@LINE+1]] range check failed
   S := Small (W);
end Conversion_Modular_Narrowing;
