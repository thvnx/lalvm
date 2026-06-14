-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- `in out` parameters are passed by reference (memref<T>): the callee reads
-- and writes through the caller's alloca pointer. No write-back is needed.

-- MLIR-LABEL: ada.subp @param_in_out
-- MLIR:         %[[N_PTR:.*]] = ada.alloca : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         ada.decls {
-- MLIR:           ada.subp private @param_in_out.increment(%arg0: memref<!ada.qual<i32, @standard.integer>>)
-- MLIR:             %[[V:.*]] = memref.load %arg0[] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:             %[[R:.*]] = ada.binop "+" %[[V]], {{.*}} checks<overflow> : !ada.qual<i32, @standard.integer>
-- MLIR:             memref.store %[[R]], %arg0[] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:             ada.return
-- MLIR:         ada.call @param_in_out.increment(%[[N_PTR]]) : (memref<!ada.qual<i32, @standard.integer>>) -> ()
-- MLIR:         %[[N:.*]] = memref.load %[[N_PTR]][] : memref<!ada.qual<i32, @standard.integer>>
-- MLIR:         ada.return %[[N]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_param_in_out(
-- LLVM-LABEL: define internal void @param_in_out__increment(ptr
-- LLVM:          #dbg_declare(ptr %0,
-- LLVM:          load i32, ptr
-- LLVM:          add i32
-- LLVM:          store i32
-- LLVM:          ret void
-- LLVM:          DILocalVariable(name: "x", arg: 1,

function Param_In_Out return Integer is
   procedure Increment (X : in out Integer);

   procedure Increment (X : in out Integer) is
   begin
      X := X + 1;
   end Increment;
   N : Integer := 41;
begin
   Increment (N);
   return N;
end Param_In_Out;
