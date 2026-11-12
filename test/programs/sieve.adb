-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %t; test $? -eq 95

-- Sieve of Eratosthenes (RM 3.6, RM 5.5): 95 primes are below 500.

function Sieve return Integer is
   Limit    : constant := 499;
   type Sieve is array (2 .. Limit) of Boolean;
   Is_Prime : Sieve;
   Multiple : Integer;
   Count    : Integer := 0;
begin
   for I in 2 .. Limit loop
      Is_Prime (I) := True;
   end loop;
   for I in 2 .. Limit loop
      exit when I * I > Limit;
      if Is_Prime (I) then
         Multiple := I * I;
         while Multiple <= Limit loop
            Is_Prime (Multiple) := False;
            Multiple := Multiple + I;
         end loop;
      end if;
   end loop;
   for I in 2 .. Limit loop
      if Is_Prime (I) then
         Count := Count + 1;
      end if;
   end loop;
   return Count;
end Sieve;
