-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Loop bounds, to the loop parameter type.

-- CHECK: ada.coerce %{{.*}} : <i32, @for_loop_bounds.small> to <i32, @standard.integer>

procedure For_Loop_Bounds (N : Integer) is
   subtype Small is Integer range 1 .. 10;
   Lo : Small := 1;
begin
   for I in Lo .. N loop
      null;
   end loop;
end For_Loop_Bounds;
