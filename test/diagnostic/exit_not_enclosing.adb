-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- An `exit` naming a loop that does not enclose it (RM 5.7): the name
-- resolves (the loop exists in the same body), so Libadalang does not reject
-- it; the `loopStack` match in MLIRGen must. An *undefined* loop name, by
-- contrast, is rejected by name resolution before MLIRGen runs.

-- CHECK: error: no enclosing loop named 'first'

procedure Exit_Not_Enclosing is
begin
   First : loop
      exit;
   end loop First;
   Second : loop
      exit First;
   end loop Second;
end Exit_Not_Enclosing;
