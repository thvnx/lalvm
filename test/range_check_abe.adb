-- RUN: %lalvm --emit=llvm %s 2>&1 | %FileCheck %s
-- XFAIL: *

-- Calling P before its body is elaborated is access-before-elaboration
-- (RM 3.11): P's body constrains a value to the later dynamic subtype S, so
-- the descriptor lifted onto the early call cannot dominate it. Follow GNAT --
-- warn and rewrite the call to a Program_Error raise, rather than failing the
-- dominance verifier.
--
-- XFAIL until subprogram specs are visited: today the forward spec
-- `function P return Integer;` is skipped, so the early call fails to resolve
-- (`unknown subprogram`) and the descriptor-on-an-early-call IR this would
-- detect never forms.
-- CHECK:      warning: cannot call "p" before body seen
-- CHECK:      warning: Program_Error will be raised at run time
-- CHECK:      call void @__gnat_rcheck_PE_Access_Before_Elaboration(

function Range_Check_Abe return Integer is
   function P return Integer;
   A : Integer := P;
   N : Integer := 10;
   subtype S is Integer range 1 .. N;
   function P return Integer is
      Y : S := 5;
   begin
      return Y;
   end P;
begin
   return A;
end Range_Check_Abe;
