-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- A nested subprogram referencing an outer dynamic subtype: the range
-- descriptor is elaborated once in the outer body and reused in the inner one
-- as an up-level reference, which ClosureConversion lifts into a parameter.
-- MLIR:      ada.range %{{.*}}, %{{.*}} : !ada.range<i32, @range_check_capture.s>
-- MLIR:      ada.range_check %{{.*}}, %{{.*}} : !ada.qual<i32, @range_check_capture.s>

-- LLVM:      call void @__gnat_rcheck_CE_Range_Check(

function Range_Check_Capture (N : Integer) return Integer is
   subtype S is Integer range 1 .. N;
   function Inner (X : Integer) return Integer is
      Y : S := X;
   begin
      return Y;
   end Inner;
begin
   return Inner (5);
end Range_Check_Capture;
