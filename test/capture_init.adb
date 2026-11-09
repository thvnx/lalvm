-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm -O0 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- The capturing call lives in an object initializer (`Y := I (True)`), which is
-- emitted into the body *before* the `ada.decls` holding `I`; closure
-- conversion still rewrites that call to forward the captured variable. `X` is
-- a plain (non-constant) local, so the capture is by reference (ptr): its
-- address escapes through the call and `I` loads through the pointer.

-- LLVM-LABEL: define i32 @_ada_capture_init(i32 %0)
-- LLVM:         %[[X:.*]] = alloca i32
-- LLVM:         store i32 %0, ptr %[[X]]
-- LLVM:         %[[Y:.*]] = call i32 @capture_init__i(i1 true, ptr %[[X]])
-- LLVM:         store i32 %[[Y]], ptr %[[YS:.*]]
-- LLVM:         %[[R:.*]] = load i32, ptr %[[YS]]
-- LLVM:         ret i32 %[[R]]
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
