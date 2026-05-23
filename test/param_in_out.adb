-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- `in out` parameters are passed by reference (memref<T>): the callee reads
-- and writes through the caller's alloca pointer. No write-back is needed.

-- MLIR-LABEL: ada.subp @test_param_in_out
-- MLIR:         ada.subp @increment(%arg0: memref<i32>)
-- MLIR:           %[[V:.*]] = memref.load %arg0[] : memref<i32>
-- MLIR:           %[[R:.*]] = ada.binop "+" %[[V]], {{.*}} : i32
-- MLIR:           memref.store %[[R]], %arg0[] : memref<i32>
-- MLIR:           ada.return
-- MLIR:         %[[N_PTR:.*]] = memref.alloca() : memref<i32>
-- MLIR:         ada.call @increment(%[[N_PTR]]) : (memref<i32>) -> ()
-- MLIR:         %[[N:.*]] = memref.load %[[N_PTR]][] : memref<i32>
-- MLIR:         ada.return %[[N]] : i32

-- LLVM-LABEL: define i32 @_ada_test_param_in_out(
-- LLVM-LABEL: define void @test_param_in_out__increment(ptr
-- LLVM:          #dbg_declare(ptr %0,
-- LLVM:          load i32, ptr
-- LLVM:          add i32
-- LLVM:          store i32
-- LLVM:          ret void
-- LLVM:          DILocalVariable(name: "X", arg: 1,

function Test_Param_In_Out return Integer is
   procedure Increment (X : in out Integer);

   procedure Increment (X : in out Integer) is
   begin
      X := X + 1;
   end Increment;
   N : Integer := 41;
begin
   Increment (N);
   return N;
end Test_Param_In_Out;
