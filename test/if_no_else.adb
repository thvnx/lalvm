-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm -O1 --emit=llvm --mlir-print-ir-after=mem2reg %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O0 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- If statement with no else (RM 5.3). After mem2reg, R is a block argument of
-- the merge block so the canonicalizer then turns the branch into a select.

-- MLIR-LABEL: IR Dump After {{.*}}mem2reg
-- MLIR:         ada.cmp "=" %x, %{{.*}} : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i1, @standard.boolean>
-- MLIR:         ada.unwrap %{{.*}} : !ada.qual<i1, @standard.boolean> to i1
-- MLIR:         cf.cond_br
-- MLIR:         cf.br ^{{bb[0-9]+}}(
-- MLIR:       ^{{bb[0-9]+}}(%{{.*}}: !ada.qual<i32, @standard.integer>)
-- MLIR:         return

-- LLVM-LABEL: define i32 @_ada_if_no_else(
-- LLVM:         store i32 %0, ptr %[[R:.*]]
-- LLVM:         icmp eq i32 %0, 0
-- LLVM:         br i1
-- LLVM:         store i32 1, ptr %[[R]]
-- LLVM:         %[[V:.*]] = load i32, ptr %[[R]]
-- LLVM:         ret i32 %[[V]]

function If_No_Else (X : Integer) return Integer is
   R : Integer := X;
begin
   if X = 0 then
      R := 1;
   end if;
   return R;
end If_No_Else;
