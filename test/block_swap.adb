-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- MLIR-LABEL: ada.proc @test
-- MLIR:         ada.block_stmt "Swap" {
-- MLIR-NEXT:    }
-- MLIR:         ada.return

-- LLVM-LABEL: define void @test(
-- LLVM:          ret void

-- Note: the Swap block body is empty at the MLIR level. In the current pure-SSA
-- model, assignments only rebind names in the compiler's symbol table without
-- emitting ops. Propagating those rebindings back to the enclosing scope (U, V)
-- after the block exits requires the alloca-based model.

procedure Test is
   U, V : Integer := 0;
begin
   Swap:
      declare
         Temp : Integer;
      begin
         Temp := V; V := U; U := Temp;
      end Swap;
end Test;
