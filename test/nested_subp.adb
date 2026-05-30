-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.subp @nested_subp
-- MLIR:         ada.subp @inner
-- MLIR:           ada.null
-- MLIR-NEXT:      ada.return
-- MLIR:         ada.call @inner() : () -> ()
-- MLIR:         ada.return

-- LLVM-LABEL: define void @_ada_nested_subp(
-- LLVM:          call void @nested_subp__inner()
-- LLVM-LABEL: define void @nested_subp__inner(

procedure Nested_Subp is
   procedure Inner;

   procedure Inner is
   begin
      null;
   end Inner;
begin
   Inner;
end Nested_Subp;
