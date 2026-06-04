-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- A nested function reads a variable from the enclosing scope (an up-level
-- reference). With `ada.subp` no longer `IsolatedFromAbove`, the read is an
-- ordinary cross-region SSA use that verifies. Here `X : constant := A` folds
-- through mem2reg to the enclosing subprogram's parameter, so `I` returns the
-- outer `%arg0` directly. The capture is still un-lifted; closure conversion
-- later turns it into a parameter and hoists `I` to module level.
-- CHECK-LABEL: ada.subp @nested_subp_upvar(%arg0: !ada.qual<i32, @standard.integer>)
-- CHECK:         ada.decls {
-- CHECK:           ada.subp private @nested_subp_upvar.i(%arg1: !ada.qual<i1, @standard.boolean>) -> !ada.qual<i32, @standard.integer>
-- CHECK:             ada.return %arg0 : !ada.qual<i32, @standard.integer>
-- CHECK:         ada.call @nested_subp_upvar.i(

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
