-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %not %lalvm --emit=obj %s -o %t.o 2>&1 | %FileCheck %s --check-prefix=MISSING

-- EXEC: %lalvm --emit=obj %s -o %t.o
-- EXEC: %lalvm --bind %s -o %t.b.o
-- EXEC: %cc %t.o %t.b.o %gnatstub -o %t
-- EXEC: %t; test $? -eq 252

-- A vote histogram: an array indexed by an enumeration type (RM 3.6).
-- Missing today: enumeration indexes, aggregates, loops over a type, 'Pos.

function Histogram return Integer is
   type Fruit is (Apple, Banana, Cherry, Date);
   type Ballots is array (1 .. 12) of Fruit;
   -- MISSING: histogram.adb:[[#@LINE+1]]:4: error: array index constraint must be a .. binary op
   type Tally is array (Fruit) of Natural;
   Votes  : constant Ballots :=
     (Cherry,
      Apple,
      Cherry,
      Date,
      Banana,
      Cherry,
      Apple,
      Cherry,
      Date,
      Cherry,
      Apple,
      Banana);
   Count  : Tally := (others => 0);
   Winner : Fruit := Fruit'First;
begin
   for I in Votes'Range loop
      Count (Votes (I)) := Count (Votes (I)) + 1;
   end loop;
   for F in Fruit loop
      if Count (F) > Count (Winner) then
         Winner := F;
      end if;
   end loop;
   --  Cherry wins with 5 of the (3, 2, 5, 2) votes.
   return Fruit'Pos (Winner) * 100 + Count (Winner) * 10 + Count (Date);
end Histogram;
