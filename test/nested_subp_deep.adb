-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @nested_subp_deep
-- MLIR:         ada.subp @a
-- MLIR:           ada.subp @b
-- MLIR:             ada.subp @c
-- MLIR:               ada.return
-- MLIR:             ada.return
-- MLIR:           ada.return
-- MLIR:         ada.return

-- Hoisting moves innermost procs to module end first (post-order), so the
-- LLVM output order is: outer, then innermost-first.
-- LLVM-LABEL: define void @_ada_nested_subp_deep(
-- LLVM-LABEL: define void @nested_subp_deep__a__b__c(
-- LLVM-LABEL: define void @nested_subp_deep__a__b(
-- LLVM-LABEL: define void @nested_subp_deep__a(

procedure Nested_Subp_Deep is
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
end Nested_Subp_Deep;
