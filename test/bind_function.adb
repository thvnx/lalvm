-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --bind --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --bind --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: module @b_bind_function
-- MLIR:         llvm.func @_ada_bind_function() -> i32
-- MLIR:         llvm.func @main(%arg0: i32, %arg1: !llvm.ptr) -> i32 {
-- MLIR:           %[[R:.*]] = llvm.call @_ada_bind_function() : () -> i32
-- MLIR:           llvm.return %[[R]] : i32

-- LLVM:      declare i32 @_ada_bind_function()
-- LLVM:      define i32 @main(i32 %{{.*}}, ptr %{{.*}}) {
-- LLVM-NEXT:   %[[R:.*]] = call i32 @_ada_bind_function()
-- LLVM-NEXT:   ret i32 %[[R]]

function Bind_Function return Integer is
begin
   return 42;
end Bind_Function;
