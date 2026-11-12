-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %not %lalvm --emit=obj %s -o %t.o 2>&1 | %FileCheck %s --check-prefix=MISSING

-- EXEC: %lalvm --emit=obj %s -o %t.o
-- EXEC: %lalvm --bind %s -o %t.b.o
-- EXEC: %cc %t.o %t.b.o %gnatstub -o %t
-- EXEC: %t; test $? -eq 48

-- A recursive descent evaluator over a String constant (RM 3.6.3).
-- Missing today: strings, case, forward declarations, 'Pos.

function Calculator return Integer is
   -- MISSING: calculator.adb:[[#@LINE+1]]:20: error: array type used before its declaration
   Text : constant String := "7*(5+3)-(20-4)/2";
   Next : Positive := Text'First;

   function Expression return Integer;

   --  Factor ::= Number | ( Expression )
   function Factor return Integer is
      Zero  : constant := Character'Pos ('0');
      Value : Integer := 0;
   begin
      if Text (Next) = '(' then
         Next := Next + 1;
         Value := Expression;
         Next := Next + 1;
         return Value;
      end if;
      while Next <= Text'Last loop
         case Text (Next) is
            when '0' .. '9' =>
               Value := Value * 10 + Character'Pos (Text (Next)) - Zero;
               Next := Next + 1;

            when others     =>
               exit;
         end case;
      end loop;
      return Value;
   end Factor;

   --  Term ::= Factor { (* | /) Factor }
   function Term return Integer is
      Value : Integer := Factor;
   begin
      while Next <= Text'Last loop
         case Text (Next) is
            when '*'    =>
               Next := Next + 1;
               Value := Value * Factor;

            when '/'    =>
               Next := Next + 1;
               Value := Value / Factor;

            when others =>
               exit;
         end case;
      end loop;
      return Value;
   end Term;

   --  Expression ::= Term { (+ | -) Term }
   function Expression return Integer is
      Value : Integer := Term;
   begin
      while Next <= Text'Last loop
         case Text (Next) is
            when '+'    =>
               Next := Next + 1;
               Value := Value + Term;

            when '-'    =>
               Next := Next + 1;
               Value := Value - Term;

            when others =>
               exit;
         end case;
      end loop;
      return Value;
   end Expression;
begin
   return Expression;
end Calculator;
