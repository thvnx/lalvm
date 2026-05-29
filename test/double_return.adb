-- XFAIL: *
-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- Known limitation: multiple return statements in the same block should
-- generate separate basic blocks with an unreachable-code warning for
-- statements following the first return.

-- CHECK-NOT: error: 'ada.return' op must be the last operation in the parent block

procedure Double_Return is
   function Test return Integer is
   begin
      return 4;
      return 5;
   end Test;
begin
   null;
end Double_Return;
