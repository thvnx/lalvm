-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- A nested subprogram that both captures an outer variable and calls itself:
-- closure conversion appends the lifted capture to the recursive call inside
-- the body (`nested_subp_recursive.adb` recurses without capturing).

-- CHECK-LABEL: define internal i32 @capture_recursive__f(i32 %0, ptr %1)
-- CHECK-DAG:     load i32, ptr %1
-- CHECK-DAG:     call i32 @capture_recursive__f(i32 %{{.*}}, ptr %1)

function Capture_Recursive (A : Integer) return Integer is
   X : Integer := A;

   function F (N : Integer) return Integer is
   begin
      if N = 0 then
         return X;
      end if;
      return F (N - 1);
   end F;
begin
   return F (3);
end Capture_Recursive;
