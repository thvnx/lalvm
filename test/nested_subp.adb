-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.proc @test_nested_subp
-- MLIR:         ada.proc @inner
-- MLIR:           ada.return
-- MLIR:         ada.call @inner() : () -> ()
-- MLIR:         ada.return

-- LLVM-LABEL: define void @test_nested_subp(
-- LLVM:          call void @test_nested_subp__inner()
-- LLVM-LABEL: define void @test_nested_subp__inner(

procedure Test_Nested_Subp is
   procedure Inner;

   procedure Inner is
   begin
      null;
   end Inner;
begin
   Inner;
end Test_Nested_Subp;
