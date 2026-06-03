-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- Two consecutive return statements: the second is unreachable, so it is
-- dropped with a warning and the function still compiles to valid IR.
-- CHECK: warning: unreachable code
-- CHECK: ada.subp private @double_return.test
-- CHECK: ada.constant : !ada.qual<i32, @standard.integer> = 4
-- CHECK: ada.return
-- CHECK-NOT: ada.constant : !ada.qual<i32, @standard.integer> = 5

procedure Double_Return is
   function Test return Integer is
   begin
      return 4;
      return 5;
   end Test;
begin
   null;
end Double_Return;
