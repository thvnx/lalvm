-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.proc @test_nested_subp_args
-- MLIR:         ada.proc @inner
-- MLIR:           ada.return
-- MLIR:         [[V:%.*]] = arith.constant 42
-- MLIR:         ada.call @inner([[V]]) : (i32) -> ()
-- MLIR:         ada.return

-- LLVM-LABEL: define void @test_nested_subp_args(
-- LLVM:          call void @test_nested_subp_args__inner(i32 42)
-- LLVM-LABEL: define void @test_nested_subp_args__inner(i32

procedure Test_Nested_Subp_Args is
   procedure Inner (X : Integer);

   procedure Inner (X : Integer) is
      I : Integer := X;
   begin
      null;
   end Inner;
begin
   Inner (42);
end Test_Nested_Subp_Args;
