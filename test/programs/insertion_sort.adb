-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %not %lalvm --emit=obj %s -o %t.o 2>&1 | %FileCheck %s --check-prefix=MISSING

-- EXEC: %lalvm --emit=obj %s -o %t.o
-- EXEC: %lalvm --bind %s -o %t.b.o
-- EXEC: %cc %t.o %t.b.o %gnatstub -o %t
-- EXEC: %t; test $? -eq 218

-- Insertion sort of an `in out` array parameter (RM 6.2).
-- Missing today: array parameters and aggregates, short-circuit forms.

function Insertion_Sort return Integer is
   type Vector is array (1 .. 8) of Integer;
   -- MISSING: insertion_sort.adb:[[#@LINE+1]]:21: error: array object initialization is not supported
   Data : Vector := (5, 2, 9, 1, 7, 3, 8, 2);

   procedure Sort (V : in out Vector) is
      Key : Integer;
      J   : Integer;
   begin
      for I in V'First + 1 .. V'Last loop
         Key := V (I);
         J := I;
         while J > V'First and then V (J - 1) > Key loop
            V (J) := V (J - 1);
            J := J - 1;
         end loop;
         V (J) := Key;
      end loop;
   end Sort;

   Checksum : Integer := 0;
begin
   Sort (Data);
   for I in Data'Range loop
      if I > Data'First and then Data (I - 1) > Data (I) then
         return 0;
      end if;
      Checksum := Checksum + I * Data (I);
   end loop;
   return Checksum;
end Insertion_Sort;
