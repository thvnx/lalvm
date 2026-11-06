-- RUN: %not %lalvm --emit=llvm %s 2>&1 | %FileCheck %s

-- Unsupported recursive capture edges must report the closure diagnostic.
-- CHECK: error: closure conversion does not support a capturing nested subprogram called from a mutually recursive or enclosing nested subprogram

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
