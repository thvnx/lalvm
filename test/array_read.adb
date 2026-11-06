-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project
--
-- Reading an array element: `ada.index` gives the element's location, read by
-- an `ada.read`. It lowers to a `getelementptr` with the zero-based offset,
-- here the constant 2 (`3 - 'First`).

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR: %[[ELT:.*]] = ada.index %{{.*}}[%{{.*}}] : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @array_read.vec>>, !ada.qual<i32, @standard.integer>) -> !ada.ref<!ada.qual<i32, @standard.integer>>
-- MLIR: ada.read %[[ELT]] : !ada.ref<!ada.qual<i32, @standard.integer>> -> !ada.qual<i32, @standard.integer>

-- LLVM: %[[ARR:.*]] = alloca [10 x i32]
-- LLVM: %[[ELT:.*]] = getelementptr [10 x i32], ptr %[[ARR]], i32 0, i64 2
-- LLVM: load i32, ptr %[[ELT]]

procedure Array_Read is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
   X : Integer;
begin
   X := V (3);
end Array_Read;
