-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %not %lalvm --emit=obj %s -o %t.o 2>&1 | %FileCheck %s --check-prefix=MISSING

-- EXEC: %lalvm --emit=obj %s -o %t.o
-- EXEC: %lalvm --bind %s -o %t.b.o
-- EXEC: %cc %t.o %t.b.o %gnatstub -o %t
-- EXEC: %t; test $? -eq 134

-- An enumeration state machine: case statements (RM 5.4), 'Succ/'Pos/'Val
-- (RM 3.5.5), membership (RM 4.5.2), `for L in Light` (RM 5.5).
-- Missing today: all four.

function Traffic_Light return Integer is
   type Light is (Red, Red_Amber, Green, Amber);
   subtype Waiting is Light range Red .. Red_Amber;

   function Next (L : Light) return Light is
   begin
      case L is
         when Amber  =>
            return Red;

         when others =>
            return Light'Succ (L);
      end case;
   end Next;

   function Seconds (L : Light) return Positive is
   begin
      case L is
         when Red               =>
            return 30;

         when Red_Amber | Amber =>
            return 3;

         when Green             =>
            return 25;
      end case;
   end Seconds;

   Current : Light := Red;
   Stopped : Natural := 0;
   Weight  : Natural := 0;
begin
   --  Two full cycles; cars stop on Red, Red_Amber and Amber (36 s each).
   for Step in 1 .. 8 loop
      -- MISSING: traffic_light.adb:[[#@LINE+1]]:10: error: unsupported expression: <MembershipExpr
      if Current in Waiting or Current = Amber then
         Stopped := Stopped + Seconds (Current);
      end if;
      Current := Next (Current);
   end loop;
   for L in Light loop
      Weight := Weight + Light'Pos (L) * Seconds (L);
   end loop;
   if Current /= Red or Light'Val (2) /= Green then
      return 0;
   end if;
   return Stopped + Weight;
end Traffic_Light;
