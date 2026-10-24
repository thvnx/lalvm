-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- Loop bounds, to the loop parameter type.

-- CHECK: ada.convert implicit %{{.*}} : !ada.qual<i32, @for_loop_bounds.small> to !ada.qual<i32, @standard.integer>

-- LOWERED: ada.requalify %{{.*}} : !ada.qual<i32, @for_loop_bounds.small> to !ada.qual<i32, @standard.integer>

procedure For_Loop_Bounds (N : Integer) is
   subtype Small is Integer range 1 .. 10;
   Lo : Small := 1;
begin
   for I in Lo .. N loop
      null;
   end loop;
end For_Loop_Bounds;
