-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- `out` parameters are passed by reference (memref<T>) like `in out`, but the
-- callee treats the initial value as undefined (RM 6.4.1). The first operation
-- on the parameter must be a store, not a load.

-- MLIR-LABEL: ada.subp @test_param_out
-- MLIR:         ada.subp @get_value(%arg0: memref<i32>)
-- MLIR-NOT:       memref.load %arg0
-- MLIR:           memref.store {{.*}}, %arg0[] : memref<i32>
-- MLIR:           ada.return
-- MLIR:         %[[N_PTR:.*]] = memref.alloca() : memref<i32>
-- MLIR-NOT:     memref.store {{.*}}, %[[N_PTR]]
-- MLIR:         ada.call @get_value(%[[N_PTR]]) : (memref<i32>) -> ()
-- MLIR:         %[[N:.*]] = memref.load %[[N_PTR]][] : memref<i32>
-- MLIR:         ada.return %[[N]] : i32

-- LLVM-LABEL: define i32 @_ada_test_param_out(
-- LLVM-LABEL: define void @test_param_out__get_value(ptr
-- LLVM:          #dbg_declare(ptr %0,
-- LLVM-NOT:     load
-- LLVM:          store i32 42, ptr
-- LLVM:          ret void
-- LLVM:          DILocalVariable(name: "X", arg: 1,

function Test_Param_Out return Integer is
   procedure Get_Value (X : out Integer);

   procedure Get_Value (X : out Integer) is
   begin
      X := 42;
   end Get_Value;
   N : Integer;
begin
   Get_Value (N);
   return N;
end Test_Param_Out;
