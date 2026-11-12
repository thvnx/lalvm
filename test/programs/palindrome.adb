-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %not %lalvm --emit=obj %s -o %t.o 2>&1 | %FileCheck %s --check-prefix=MISSING

-- EXEC: %lalvm --emit=obj %s -o %t.o
-- EXEC: %lalvm --bind %s -o %t.b.o
-- EXEC: %cc %t.o %t.b.o %gnatstub -o %t
-- EXEC: %t; test $? -eq 45

-- Palindromes: string literals passed to String formals (RM 3.6.3, RM 6.2).
-- Missing today: unconstrained arrays (String).

function Palindrome return Integer is
   -- MISSING: palindrome.adb:[[#@LINE+1]]:32: error: array type used before its declaration
   function Is_Palindrome (S : String) return Boolean is
   begin
      for I in 0 .. S'Length / 2 - 1 loop
         if S (S'First + I) /= S (S'Last - I) then
            return False;
         end if;
      end loop;
      return True;
   end Is_Palindrome;

   function Equals_Reversed (S : String) return Boolean is
      R : String (S'Range);
   begin
      for I in S'Range loop
         R (S'Last - (I - S'First)) := S (I);
      end loop;
      return R = S;
   end Equals_Reversed;

   Found : Natural := 0;

   procedure Check (S : String; Weight : Positive) is
   begin
      if Is_Palindrome (S) and Equals_Reversed (S) then
         Found := Found + Weight;
      end if;
   end Check;
begin
   Check ("racecar", 1);
   Check ("lalvm", 2);
   Check ("level", 4);
   Check ("", 8);
   Check ("ab", 16);
   Check ("noon", 32);
   return Found;
end Palindrome;
