-- XFAIL: *
-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- Known limitation: nested subprograms cannot access variables declared in
-- an enclosing scope (upward variable references). Requires static link
-- support: passing a pointer to the enclosing frame and rewriting accesses
-- to enclosed variables as loads through that pointer.

-- CHECK-NOT: error: 'ada.return' op using value defined outside the region

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
