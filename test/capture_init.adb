-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- The capturing call lives in an object initializer (`Y := I (True)`), which is
-- emitted into the body *before* the `ada.decls` holding `I`; closure
-- conversion still rewrites that call to forward the captured variable. `X` is
-- a plain (non-constant) local, so the capture is by reference (ptr): its
-- address escapes through the call, mem2reg keeps it in memory, and `I` loads
-- through the pointer. The function returns its argument unchanged.

-- LLVM-LABEL: define i32 @_ada_capture_init(i32 %0)
-- LLVM:         %[[X:.*]] = alloca i32
-- LLVM:         store i32 %0, ptr %[[X]]
-- LLVM:         %[[Y:.*]] = call i32 @capture_init__i(i1 true, ptr %[[X]])
-- LLVM:         ret i32 %[[Y]]
-- LLVM-LABEL: define internal i32 @capture_init__i(i1 %0, ptr %1)
-- LLVM:         %[[V:.*]] = load i32, ptr %1
-- LLVM:         ret i32 %[[V]]

function Capture_Init (A : Integer) return Integer is
   X : Integer := A;

   function I (B : Boolean) return Integer is
   begin
      return X;
   end I;

   Y : Integer := I (True);
begin
   return Y;
end Capture_Init;
