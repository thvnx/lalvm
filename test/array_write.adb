-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project
--
-- Writing an array element: `ada.index` gives the element's location, written
-- by a `memref.store`. It lowers to a `getelementptr` with the zero-based
-- offset, here the constant 2 (`3 - 'First`).

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR: %[[ELT:.*]] = ada.index %{{.*}}[%{{.*}}] : (memref<!ada.qual<!ada.array<i32[i32 x 10]>, @array_write.vec>>, !ada.qual<i32, @standard.integer>) -> memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>
-- MLIR: memref.store %{{.*}}, %[[ELT]][] : memref<!ada.qual<i32, @standard.integer>, strided<[], offset: ?>>

-- LLVM: %[[ARR:.*]] = alloca [10 x i32]
-- LLVM: %[[ELT:.*]] = getelementptr [10 x i32], ptr %[[ARR]], i32 0, i32 2
-- LLVM: store i32 {{.*}}, ptr %[[ELT]]

procedure Array_Write is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
begin
   V (3) := 42;
end Array_Write;
