-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @nested_subp_args
-- MLIR:         ada.subp private @nested_subp_args.inner
-- MLIR:           ada.return
-- MLIR:         [[V:%.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 42
-- MLIR:         ada.call @nested_subp_args.inner([[V]]) : (!ada.qual<i32, @standard.integer>) -> ()
-- MLIR:         ada.return

-- LLVM-LABEL: define void @_ada_nested_subp_args(
-- LLVM:          call void @nested_subp_args__inner(i32 42)
-- LLVM-LABEL: define internal void @nested_subp_args__inner(i32

procedure Nested_Subp_Args is
   procedure Inner (X : Integer);

   procedure Inner (X : Integer) is
      I : Integer := X;
   begin
      null;
   end Inner;
begin
   Inner (42);
end Nested_Subp_Args;
