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

-- RUN: %lalvm --emit=llvm -g %s | %FileCheck %s --check-prefix=DI
-- DI-DAG: ![[INTEGER:[0-9]+]] = !DIBasicType(name: "integer"
-- DI-DAG: ![[TOUCH:[0-9]+]] = distinct !DISubprogram(name: "touch", {{.*}}type: ![[TOUCH_TYPE:[0-9]+]]
-- DI-DAG: ![[TOUCH_TYPE]] = !DISubroutineType({{.*}}types: ![[TOUCH_TYPES:[0-9]+]])
-- DI-DAG: ![[TOUCH_TYPES]] = !{null}
-- DI-DAG: ![[OUTER:[0-9]+]] = distinct !DISubprogram(name: "nested_formal", {{.*}}type: ![[OUTER_TYPE:[0-9]+]]
-- DI-DAG: ![[OUTER_TYPE]] = !DISubroutineType({{.*}}types: ![[OUTER_TYPES:[0-9]+]])
-- DI-DAG: ![[OUTER_TYPES]] = !{null, ![[INTEGER]]}
-- DI-DAG: !DILocalVariable(name: "v", arg: 1, scope: ![[OUTER]],

function Call_Values return Integer is
   X       : Integer := 1;
   Count   : Integer := 0;
   Current : Integer := 1;
   I, J    : Integer := 1;
   type Vector is array (1 .. 2) of Integer;
   Items   : Vector;

   function Change return Integer is
   begin
      X := 9;
      return 2;
   end Change;

   procedure Snapshot (A : in out Integer; B : Integer) is
   begin
      A := A + B;
   end Snapshot;

   procedure Independent (A, B : in out Integer) is
   begin
      A := 5;
      B := B + 1;
   end Independent;

   function Index return Integer is
   begin
      Count := Count + 1;
      return Current;
   end Index;

   function Change_Index return Integer is
   begin
      Current := 2;
      return 4;
   end Change_Index;

   function Results (A : out Integer; B : in out Integer) return Integer is
   begin
      A := 11;
      B := B + 1;
      return 7;
   end Results;

   procedure Nested_Formal (V : in out Integer) is
      procedure Touch is
      begin
         V := V + 2;
      end Touch;
   begin
      Touch;
   end Nested_Formal;

   function Accumulate (N : Integer; A : in out Integer) return Integer is
   begin
      if N = 0 then
         return A;
      end if;
      A := A + 1;
      return Accumulate (N - 1, A);
   end Accumulate;

   type Choice is (First, Second);
   Flag       : Boolean := False;
   Selected   : Choice := First;
   Real_Value : Float := 1.0;
   Wide       : Long_Integer := 4;
   procedure Change_Kinds
     (B : out Boolean;
      E : in out Choice;
      F : in out Float;
      W : in out Long_Integer) is
   begin
      B := True;
      E := Second;
      F := F + 0.5;
      W := W + 1;
   end Change_Kinds;

   subtype Small is Integer range 1 .. 20;
   Y : Small := 4;
   R : Integer;
begin
   -- A receives the snapshot taken before Change modifies X.
   Snapshot (X, Change);
   if X /= 3 then
      return 1;
   end if;
   -- Named associations select B first, so Change precedes the snapshot of X.
   X := 1;
   Snapshot (B => Change, A => X);
   if X /= 11 then
      return 2;
   end if;
   -- Distinct index variables permit runtime aliasing (RM 6.4.1(6.9, 6.17)).
   -- Formal objects remain independent, then results follow association order.
   Items (1) := 1;
   Independent (A => Items (I), B => Items (J));
   if Items (1) /= 2 then
      return 3;
   end if;
   Items (1) := 1;
   Independent (B => Items (J), A => Items (I));
   if Items (1) /= 5 then
      return 4;
   end if;
   -- The destination designator is evaluated once, including during copyback.
   Items (1) := 3;
   Items (2) := 50;
   Snapshot (Items (Index), Change_Index);
   if Count /= 1 then
      return 5;
   end if;
   if Items (1) /= 7 then
      return 5;
   end if;
   if Items (2) /= 50 then
      return 5;
   end if;
   -- The function result precedes the writable formal results.
   R := Results (X, Y);
   if R /= 7 then
      return 6;
   end if;
   if X /= 11 then
      return 6;
   end if;
   if Y /= 5 then
      return 6;
   end if;
   Nested_Formal (X);
   if X /= 13 then
      return 7;
   end if;
   X := 0;
   R := Accumulate (3, X);
   if R /= 3 then
      return 8;
   end if;
   if X /= 3 then
      return 8;
   end if;
   Change_Kinds (Flag, Selected, Real_Value, Wide);
   if not Flag then
      return 9;
   end if;
   if Selected /= Second then
      return 9;
   end if;
   if Real_Value /= 1.5 then
      return 9;
   end if;
   if Wide /= 5 then
      return 9;
   end if;
   return 0;
end Call_Values;
