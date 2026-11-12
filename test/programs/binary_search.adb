-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %not %lalvm --emit=obj %s -o %t.o 2>&1 | %FileCheck %s --check-prefix=MISSING

-- EXEC: %lalvm --emit=obj %s -o %t.o
-- EXEC: %lalvm --bind %s -o %t.b.o
-- EXEC: %cc %t.o %t.b.o %gnatstub -o %t
-- EXEC: %t; test $? -eq 156

-- Binary search in a sorted constant array passed as an `in` parameter (RM 6.2).
-- Missing today: array parameters, aggregates and attributes.

function Binary_Search return Integer is
   type Table is array (1 .. 16) of Integer;
   Primes : constant Table :=
     -- MISSING: binary_search.adb:[[#@LINE+1]]:6: error: array object initialization is not supported
     (2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53);

   --  The index of Key in T, or 0 when absent.
   function Find (T : Table; Key : Integer) return Natural is
      Low  : Integer := T'First;
      High : Integer := T'Last;
      Mid  : Integer;
   begin
      while Low <= High loop
         Mid := (Low + High) / 2;
         if T (Mid) = Key then
            return Mid;
         elsif T (Mid) < Key then
            Low := Mid + 1;
         else
            High := Mid - 1;
         end if;
      end loop;
      return 0;
   end Find;

   Found : Natural := 0;
begin
   for Key in 0 .. 60 loop
      if Find (Primes, Key) /= 0 then
         Found := Found + 1;
      end if;
   end loop;
   return Find (Primes, 43) * 10 + Found;
end Binary_Search;
