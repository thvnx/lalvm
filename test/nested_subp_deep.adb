-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.proc @test_deep_nesting
-- MLIR:         ada.proc @a
-- MLIR:           ada.proc @b
-- MLIR:             ada.proc @c
-- MLIR:               ada.return
-- MLIR:             ada.return
-- MLIR:           ada.return
-- MLIR:         ada.return

-- Hoisting moves innermost procs to module end first (post-order), so the
-- LLVM output order is: outer, then innermost-first.
-- LLVM-LABEL: define void @test_deep_nesting(
-- LLVM-LABEL: define void @test_deep_nesting__a__b__c(
-- LLVM-LABEL: define void @test_deep_nesting__a__b(
-- LLVM-LABEL: define void @test_deep_nesting__a(

procedure Test_Deep_Nesting is
   procedure A is
      procedure B is
         procedure C is
         begin
            null;
         end C;
      begin
         null;
      end B;
   begin
      null;
   end A;
begin
   null;
end Test_Deep_Nesting;
