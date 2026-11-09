-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
-- RUN: %lalvm -O1 --emit=llvm --mlir-print-ir-after=ada-to-llvm %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- Mutable and transitive captures share the same object.
-- Add_Twice forwards Total through Add, preserving both writes.

-- LLVM-LABEL: define i32 @_ada_counter(
-- LLVM:         %[[T:.*]] = alloca i32
-- LLVM:         store i32 0, ptr %[[T]]
-- LLVM:         call void @counter__add_twice(i32 5, ptr %[[T]])
-- LLVM:         %[[R:.*]] = load i32, ptr %[[T]]
-- LLVM:         ret i32 %[[R]]

-- LLVM-DAG: define internal void @counter__add(i32 {{.*}}, ptr
-- LLVM-DAG: define internal void @counter__add_twice(i32 {{.*}}, ptr

-- LOWERED-LABEL: llvm.func @_ada_counter(
-- LOWERED: %[[T:.*]] = llvm.alloca
-- LOWERED: llvm.store %{{.*}}, %[[T]]
-- LOWERED: llvm.call @counter__add_twice(%{{.*}}, %[[T]])
-- LOWERED: %[[R:.*]] = llvm.load %[[T]]
-- LOWERED: llvm.return %[[R]]
-- LOWERED-DAG: llvm.func internal @counter__add(
-- LOWERED-DAG: llvm.func internal @counter__add_twice(

function Counter return Integer is
   Total : Integer := 0;

   procedure Add (N : Integer) is
   begin
      Total := Total + N;
   end Add;

   procedure Add_Twice (N : Integer) is
   begin
      Add (N);
      Add (N);
   end Add_Twice;
begin
   Add_Twice (5);
   return Total;
end Counter;
