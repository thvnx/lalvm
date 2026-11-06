-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %t
-- RUN: %lalvm --emit=obj -O1 %s -o %t.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %t

-- Captures share array identity and observe each iteration's SSA value.
function Object_Values return Integer is
   type Vector is array (1 .. 3) of Integer;
   V : Vector;
   Count : Integer := 0;
   Sum : Integer := 0;
   High : Integer := 3;

   function Bound return Integer is
   begin
      Count := Count + 1;
      return 3;
   end Bound;
begin
   for I in 1 .. Bound loop
      declare
         procedure Store is
         begin
            V (I) := I * 2;
         end Store;
      begin
         Store;
      end;
   end loop;
   if Count /= 1 then
      return 1;
   end if;
   for J in reverse 1 .. 3 loop
      declare
         function Read return Integer is
         begin
            return V (J);
         end Read;
      begin
         Sum := Sum * 10 + Read;
      end;
   end loop;
   if Sum /= 642 then
      return 2;
   end if;
   for K in High .. 1 loop
      return 3;
   end loop;
   return 0;
end Object_Values;
