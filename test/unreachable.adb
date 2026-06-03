-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- The nested blocks dissolve, so the inner `return True` terminates the
-- function; the `return False` after the outer block is unreachable and dropped
-- with a warning, leaving a single return.
-- MLIR: warning: unreachable code
-- MLIR-LABEL: ada.subp @unreachable
-- MLIR: ada.constant : !ada.qual<i1, @standard.boolean> = true
-- MLIR: ada.return
-- MLIR-NOT: ada.return

-- LLVM-LABEL: define i1 @_ada_unreachable(
-- LLVM: ret i1 true

function Unreachable return Boolean is
begin
   declare
   begin
      begin
         return True;
      end;
   end;

   return False;
end Unreachable;
