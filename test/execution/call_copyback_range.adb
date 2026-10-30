-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-calls %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=CALL --implicit-check-not=ada.subp --implicit-check-not=ada.call --implicit-check-not=ada.return
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=CONVERTED --implicit-check-not=ada.convert
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-references %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=STORAGE --implicit-check-not=!ada.ref --implicit-check-not=ada.object --implicit-check-not=ada.read --implicit-check-not=ada.write
-- CONVERTED: ada.range_check
-- STORAGE: memref.store
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

procedure Call_Copyback_Range is
   procedure Run (N : Integer) is
      subtype Small is Integer range 1 .. N;
      X : Integer := 0;
      Y : Small := 1;
      procedure Update (A, B : out Integer) is
      begin
         A := 2;
         B := 6;
      end Update;
   begin
      -- Each conversion immediately precedes its write. The second uses the
      -- actual object's saved dynamic constraint, after the first write.
      -- CALL: %[[RESULTS:.*]]:2 = call @call_copyback_range__run__update()
      -- CALL-NEXT: %[[A:.*]] = ada.convert implicit %[[RESULTS]]#0
      -- CALL-NEXT: ada.write %[[A]], %{{.*}}
      -- CALL-NEXT: %[[B:.*]] = ada.convert implicit %[[RESULTS]]#1, %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @call_copyback_range.run.small>
      -- CALL-NEXT: ada.write %[[B]], %{{.*}}
      -- CHECK: raised CONSTRAINT_ERROR : call_copyback_range.adb:[[#@LINE+1]] range check failed
      Update (X, Y);
   end Run;
begin
   Run (5);
end Call_Copyback_Range;

-- RUN: %lalvm --emit=obj -O1 %s -o %t.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out
