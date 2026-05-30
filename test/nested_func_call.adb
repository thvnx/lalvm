-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @nested_func_call
-- MLIR:         ada.subp @inner(
-- MLIR:           ada.return %{{.*}} : !ada.qual<i32, @standard.integer>
-- MLIR:         %[[C1:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 42
-- MLIR:         %[[V1:.*]] = ada.call @inner(%[[C1]]) : (!ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:         %[[C2:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 42
-- MLIR:         %[[V2:.*]] = ada.call @inner(%[[C2]]) : (!ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
-- MLIR:         ada.return

-- LLVM-LABEL: define void @_ada_nested_func_call(
-- LLVM:          call i32 @nested_func_call__inner(i32 42)
-- LLVM:          call i32 @nested_func_call__inner(i32 42)
-- LLVM:          ret void
-- LLVM-LABEL: define i32 @nested_func_call__inner(i32
-- LLVM:          ret i32 %0

procedure Nested_Func_Call is
   function Inner (X : Integer) return Integer;

   function Inner (X : Integer) return Integer is
      I : Integer := X;
   begin
      return I;
   end Inner;

   I : Integer := Inner (42);
begin
   I := Inner (42);
end Nested_Func_Call;
