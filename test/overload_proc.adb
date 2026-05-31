-- XFAIL: *
-- RUN: %lalvm --emit=llvm %s 2>&1 | %FileCheck %s

-- Known limitation: overloaded subprograms with the same name produce a
-- "redefinition of symbol" error. GNAT resolves the clash by appending a
-- `__N` suffix to each redefinition (N starting at 2), e.g. `test__p` for
-- the first `P` and `test__p__2` for the second.

-- CHECK-NOT: error:

procedure Test is

   procedure P (A : Boolean) is
      X : Boolean := A;
   begin
      null;
   end P;

   procedure P (A : Integer) is
      X : Integer := A;
   begin
      null;
   end P;

begin
   P (1);
   P (True);
end Test;
