-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- Conversion of a negative value to a modular type.

procedure Conversion_Modular is
   type Byte is mod 256;
   X : Integer := -1;
   B : Byte;
begin
   -- CHECK: raised CONSTRAINT_ERROR : conversion_modular.adb:[[#@LINE+1]] range check failed
   B := Byte (X);
end Conversion_Modular;
