-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %check-exit %t 42

-- Check operand signs, exact multiples, signed minima and modular operands.

function Mod_Rem return Integer is
   type Byte is mod 256;
   type Dec is mod 10;
   type Small is range -128 .. 127;
   C     : constant Integer := (-7) mod 3;
   P7    : Integer := 7;
   M7    : Integer := -7;
   P6    : Integer := 6;
   P3    : Integer := 3;
   M3    : Integer := -3;
   M1    : Integer := -1;
   First : Integer := Integer'First;
   B200  : Byte := 200;
   B7    : Byte := 7;
   D9    : Dec := 9;
   D4    : Dec := 4;
   S_Min : Small := Small'First;
   S_M1  : Small := -1;
   S_M3  : Small := -3;
   S10   : Small := 10;
begin
   if P7 rem P3 /= 1 then
      return 1;
   elsif M7 rem P3 /= -1 then
      return 2;
   elsif P7 rem M3 /= 1 then
      return 3;
   elsif M7 rem M3 /= -1 then
      return 4;
   elsif P7 mod P3 /= 1 then
      return 5;
   elsif M7 mod P3 /= 2 then
      return 6;
   elsif P7 mod M3 /= -2 then
      return 7;
   elsif M7 mod M3 /= -1 then
      return 8;
   elsif P6 mod M3 /= 0 then
      return 9;
   elsif First rem M1 /= 0 then
      return 10;
   elsif First mod M1 /= 0 then
      return 11;
   elsif B200 mod B7 /= 4 then
      return 12;
   elsif B200 rem B7 /= 4 then
      return 13;
   elsif C + 1 /= 3 then
      return 14;
   elsif S_Min rem S_M1 /= 0 then
      return 15;
   elsif S_Min mod S_M1 /= 0 then
      return 16;
   elsif S_Min mod S10 /= 2 then
      return 17;
   elsif S_Min rem S_M3 /= -2 then
      return 18;
   elsif D9 mod D4 /= 1 then
      return 19;
   elsif D9 rem D4 /= 1 then
      return 20;
   end if;
   return 42;
end Mod_Rem;
