-- RUN: %lalvm --emit=llvm %s 2>&1 | %FileCheck %s
-- XFAIL: *

-- Mutually recursive nested subprograms where one captures: P captures Outer's
-- X and calls its nested Q, and Q calls P back (an upward edge). The single
-- innermost-first closure-conversion pass lifts Q first (it captures nothing,
-- so it is hoisted), then P (capturing X), but the call to P left inside the
-- already-hoisted Q can no longer be given X. The pass diagnoses this instead
-- of emitting IR that fails verification; a fixpoint over the call graph would
-- lift the limitation, at which point this test should compile cleanly and
-- XPASS (drop the XFAIL then).
-- CHECK-NOT: error:

function Capture_Mutual (A : Integer) return Integer is
   X : Integer := A;

   function P (N : Integer) return Integer is
      function Q (M : Integer) return Integer is
      begin
         return P (M);
      end Q;
   begin
      return X + Q (N);
   end P;
begin
   return P (A);
end Capture_Mutual;
