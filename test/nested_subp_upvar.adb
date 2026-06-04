-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- A nested function reads a variable from the enclosing scope (an up-level
-- reference). At the dialect level it is an un-lifted cross-region SSA use:
-- `X : constant := A` folds through mem2reg to the enclosing parameter, so `I`
-- returns the outer `%arg0`. Closure conversion then lifts the capture into a
-- trailing parameter and hoists `I` to module level; the call passes it.
-- MLIR-LABEL: ada.subp @nested_subp_upvar(%arg0: !ada.qual<i32, @standard.integer>)
-- MLIR:         ada.decls {
-- MLIR:           ada.subp private @nested_subp_upvar.i(%arg1: !ada.qual<i1, @standard.boolean>) -> !ada.qual<i32, @standard.integer>
-- MLIR:             ada.return %arg0 : !ada.qual<i32, @standard.integer>
-- MLIR:         ada.call @nested_subp_upvar.i(

-- LLVM-LABEL: define void @_ada_nested_subp_upvar(i32 %0
-- LLVM:         call i32 @nested_subp_upvar__i(i1 true, i32 %0)
-- LLVM-LABEL: define internal i32 @nested_subp_upvar__i(i1 %0, i32 %1)
-- LLVM:         ret i32 %1

procedure Nested_Subp_Upvar (A : Integer) is
   X : constant Integer := A;

   function I (B : Boolean) return Integer is
   begin
      return X;
   end I;

   Z : Integer;
begin
   Z := I (True);
end Nested_Subp_Upvar;
