-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-indexing %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=INDEXING
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.err
-- RUN: %FileCheck %s --check-prefix=ERROR < %t.err
-- RUN: %lalvm --emit=obj -O1 %s -o %t.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.err
-- RUN: %FileCheck %s --check-prefix=ERROR < %t.err

-- A null array has no selectable component.
-- INDEXING: ada.index_check
-- INDEXING-NOT: ada.index_check
-- INDEXING: return

procedure Array_Index_Null is
   procedure Touch (I : Integer) is
      type Vector is array (5 .. 4) of Integer;
      V : Vector;
   begin
      -- ERROR: raised CONSTRAINT_ERROR : array_index_null.adb:[[#@LINE+1]] index check failed
      V (I) := 1;
   end Touch;
begin
   Touch (5);
end Array_Index_Null;
