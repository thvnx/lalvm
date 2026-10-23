-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Assignment.

-- CHECK: ada.range_check %arg0, %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @assignment.small>

procedure Assignment (X : Integer) is
   subtype Small is Integer range 1 .. 10;
   Y : Small;
begin
   Y := X;
end Assignment;
