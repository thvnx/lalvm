-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --bind --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --bind --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
-- RUN: %lalvm --bind --emit=asm %s -o - | %FileCheck %s --check-prefix=ASM

-- MLIR-LABEL: module @b_bind_procedure
-- MLIR:         llvm.func @_ada_bind_procedure()
-- MLIR:         llvm.func @main(%arg0: i32, %arg1: !llvm.ptr) -> i32 {
-- MLIR:           llvm.call @_ada_bind_procedure() : () -> ()
-- MLIR:           %[[ZERO:.*]] = llvm.mlir.constant(0 : i32) : i32
-- MLIR:           llvm.return %[[ZERO]] : i32

-- LLVM:      declare void @_ada_bind_procedure()
-- LLVM:      define i32 @main(i32 %{{.*}}, ptr %{{.*}}) {
-- LLVM-NEXT:   call void @_ada_bind_procedure()
-- LLVM-NEXT:   ret i32 0

-- ASM: main:
-- ASM: _ada_bind_procedure

procedure Bind_Procedure is
begin
   null;
end Bind_Procedure;
