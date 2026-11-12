-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %not %lalvm --emit=obj %s -o %t.o 2>&1 | %FileCheck %s --check-prefix=MISSING

-- EXEC: %lalvm --emit=obj %s -o %t.o
-- EXEC: %lalvm --bind %s -o %t.b.o
-- EXEC: %cc %t.o %t.b.o %gnatstub -o %t
-- EXEC: %t; test $? -eq 42

-- A postfix evaluator: a record (RM 3.8) kept private in a nested package
-- (RM 7.3), a case (RM 5.4), a `raise`.
-- Missing today: records, nested packages, aggregates, case, raise.

function Rpn_Stack return Integer is
   -- MISSING: rpn_stack.adb:[[#@LINE+1]]:4: error: unsupported declaration in declarative part: <PackageDecl ["Stacks"]
   package Stacks is
      type Stack is private;
      procedure Push (S : in out Stack; X : Integer);
      procedure Pop (S : in out Stack; X : out Integer);
      function Depth (S : Stack) return Natural;
   private
      type Values is array (1 .. 8) of Integer;
      type Stack is record
         Top  : Natural := 0;
         Data : Values;
      end record;
   end Stacks;

   package body Stacks is
      procedure Push (S : in out Stack; X : Integer) is
      begin
         S.Top := S.Top + 1;
         S.Data (S.Top) := X;
      end Push;

      procedure Pop (S : in out Stack; X : out Integer) is
      begin
         X := S.Data (S.Top);
         S.Top := S.Top - 1;
      end Pop;

      function Depth (S : Stack) return Natural is
      begin
         return S.Top;
      end Depth;
   end Stacks;

   type Kind is (Number, Add, Subtract, Multiply);
   type Token is record
      What  : Kind;
      Value : Integer := 0;
   end record;
   type Program is array (1 .. 9) of Token;

   function Apply (Op : Kind; A, B : Integer) return Integer is
   begin
      case Op is
         when Add      =>
            return A + B;

         when Subtract =>
            return A - B;

         when Multiply =>
            return A * B;

         when Number   =>
            raise Program_Error;
      end case;
   end Apply;

   --  (6 - 2) * (3 + 7) + 2, in postfix.
   Code : constant Program :=
     ((Number, 6),
      (Number, 2),
      (Subtract, 0),
      (Number, 3),
      (Number, 7),
      (Add, 0),
      (Multiply, 0),
      (Number, 2),
      (Add, 0));
   S    : Stacks.Stack;
   A, B : Integer;
begin
   for I in Code'Range loop
      if Code (I).What = Number then
         Stacks.Push (S, Code (I).Value);
      else
         Stacks.Pop (S, B);
         Stacks.Pop (S, A);
         Stacks.Push (S, Apply (Code (I).What, A, B));
      end if;
   end loop;
   Stacks.Pop (S, A);
   if Stacks.Depth (S) /= 0 then
      return 0;
   end if;
   return A;
end Rpn_Stack;
