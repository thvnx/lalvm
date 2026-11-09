-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm -O0 --emit=llvm %s | %FileCheck %s --check-prefix=O0
-- RUN: %lalvm -O --emit=llvm %s | %FileCheck %s --check-prefix=UNROLL --implicit-check-not=' = phi '
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=UNROLL --implicit-check-not=' = phi '
-- RUN: %lalvm -O2 --emit=llvm %s | %FileCheck %s --check-prefix=UNROLL --implicit-check-not=' = phi '
-- RUN: %lalvm -O3 --emit=llvm %s | %FileCheck %s --check-prefix=UNROLL --implicit-check-not=' = phi '

-- LLVM optimizes the fixed trip count loop (RM 5.5) at each enabled level.
-- The MLIR helper enables loop unrolling from -O1.

-- O0-LABEL: define i32 @_ada_opt_level_unroll(
-- O0: alloca i32

-- UNROLL-LABEL: define {{.*}}i32 @_ada_opt_level_unroll(
-- UNROLL-DAG:     @llvm.sadd.with.overflow.i32(i32 %{{.*}}, i32 1)
-- UNROLL-DAG:     @llvm.sadd.with.overflow.i32(i32 %{{.*}}, i32 2)
-- UNROLL-DAG:     @llvm.sadd.with.overflow.i32(i32 %{{.*}}, i32 3)
-- UNROLL-DAG:     @llvm.sadd.with.overflow.i32(i32 %{{.*}}, i32 4)
-- UNROLL:         ret i32

function Opt_Level_Unroll (N : Integer) return Integer is
   S : Integer := N;
begin
   for I in 1 .. 4 loop
      S := S * N + I;
   end loop;
   return S;
end Opt_Level_Unroll;
