-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o 2> %t.err
-- RUN: %FileCheck %s --check-prefix=DIAG --allow-empty < %t.err
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %t
-- RUN: %lalvm --emit=obj -O1 %s -o %t.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %t
-- DIAG-NOT: read before first assignment

-- Out formals preserve the actual value, even outside the formal subtype.
-- Returning to a narrower actual preserves its original value, not the aspect.

function Call_Default_Value return Integer is
   type T is range 0 .. 100 with Default_Value => 7;
   subtype Narrow is T range 1 .. 5;
   X : T := 9;
   Y : Narrow := 3;
   Seen : Integer := 0;
   procedure Keep (V : out Narrow) is
   begin
      Seen := Integer (V);
   end Keep;
   procedure Unchanged (V : out T) is
   begin
      null;
   end Unchanged;
begin
   Keep (X);
   if Seen /= 9 then
      return 1;
   end if;
   if X /= 9 then
      return 2;
   end if;
   Unchanged (Y);
   if Y /= 3 then
      return 3;
   end if;
   return 0;
end Call_Default_Value;
