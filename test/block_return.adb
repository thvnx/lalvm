-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- A `return` inside a block terminates the enclosing procedure once the block
-- dissolves, so the statement following the block is unreachable: it is dropped
-- with a warning. No implicit return is appended either, since the body already
-- ends in a terminator -- so the procedure has exactly one `ada.return`.
-- CHECK: warning: unreachable code
-- CHECK-LABEL: ada.subp @block_return
-- CHECK: ada.return
-- CHECK-NOT: ada.return
-- CHECK-NOT: ada.call

procedure Block_Return is
begin
   begin
      return;
   end;
   Block_Return;
end Block_Return;
