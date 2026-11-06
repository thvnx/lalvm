-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %t
-- RUN: %lalvm --emit=obj -O1 %s -o %t.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %t

-- The offset 255 must remain positive even though the Ada index uses i8.
-- CHECK: getelementptr [256 x i32], ptr %{{.*}}, i32 0, i64 255

function Array_Index_Narrow return Integer is
   type Index_Type is range -128 .. 127;
   type Vector is array (Index_Type'(-128) .. Index_Type'(127)) of Integer;
   V : Vector;
begin
   V (-128) := 1;
   V (127) := 42;
   if V (-128) /= 1 then
      return 1;
   end if;
   if V (127) /= 42 then
      return 2;
   end if;
   return 0;
end Array_Index_Narrow;
