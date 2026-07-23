-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- An `exit` outside any loop (RM 5.7) is diagnosed by MLIRGen: the statement
-- parses and name-resolves cleanly, so Libadalang does not reject it first.

-- CHECK: error: exit outside of a loop

procedure Exit_Outside_Loop is
begin
   exit;
end Exit_Outside_Loop;
