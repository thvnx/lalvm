-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- A `goto` out of a body (RM 5.8) still name-resolves (the label is
-- implicitly declared in the enclosing body, RM 5.1), so MLIRGen must
-- diagnose it. The reverse direction (into a nested body) does not
-- name-resolve and is rejected by the frontend.

-- CHECK: error: goto label outside the current subprogram

procedure Goto_Cross_Body is
   procedure Inner is
   begin
      goto Out_Label;
   end Inner;
begin
   Inner;
   <<Out_Label>>
   null;
end Goto_Cross_Body;
