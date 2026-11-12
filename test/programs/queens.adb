-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %t; test $? -eq 92

-- Eight queens by backtracking: the 92 solutions are the exit status.

function Queens return Integer is
   Size      : constant := 8;
   type Board is array (1 .. Size) of Integer;
   Column    : Board;
   Solutions : Integer := 0;

   --  No queen on an earlier row shares Col or a diagonal with Row.
   function Is_Safe (Row, Col : Integer) return Boolean is
   begin
      for R in 1 .. Row - 1 loop
         if Column (R) = Col or abs (Column (R) - Col) = Row - R then
            return False;
         end if;
      end loop;
      return True;
   end Is_Safe;

   procedure Place (Row : Integer) is
   begin
      if Row > Size then
         Solutions := Solutions + 1;
         return;
      end if;
      for Col in 1 .. Size loop
         if Is_Safe (Row, Col) then
            Column (Row) := Col;
            Place (Row + 1);
         end if;
      end loop;
   end Place;
begin
   Place (1);
   return Solutions;
end Queens;
