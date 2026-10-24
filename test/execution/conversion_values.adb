-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %t

-- Conversions in range. A wrong value fails the range check of Ok.

procedure Conversion_Values is
   type Byte is mod 256;
   B  : Byte := 200;
   S  : Short_Integer := -5;
   L  : Long_Integer := 1_000;
   I  : Integer;
   Ok : Positive;
begin
   -- Modular widening.
   I := Integer (B);
   Ok := (if I = 200 then 1 else 0);
   -- Signed widening.
   I := Integer (S);
   Ok := (if I = -5 then 1 else 0);
   -- Narrowing.
   I := Integer (L);
   Ok := (if I = 1_000 then 1 else 0);
   -- To a modular type.
   B := Byte (I - 800);
   Ok := (if B = 200 then 1 else 0);
end Conversion_Values;
