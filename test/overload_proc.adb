-- RUN: %lalvm --emit=llvm %s 2>&1 | %FileCheck %s

-- Overloaded subprograms with the same name get distinct dialect symbols: each
-- collision appends a `__N` suffix (N starting at 2). The nested `P` overloads
-- are `overload_proc.p` and `overload_proc.p__2`, mangled to `overload_proc__p`
-- and `overload_proc__p__2`.

-- CHECK-NOT: error:
-- CHECK-DAG: @overload_proc__p(
-- CHECK-DAG: @overload_proc__p__2(

procedure Overload_Proc is

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
end Overload_Proc;
