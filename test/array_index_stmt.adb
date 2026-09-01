-- An array index in statement position (`V (3);`) is not a valid statement
-- (RM 6.4). Libadalang reports no precise diagnostics for that error for now.

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: error: {{.*}}name resolution failed but no diagnostics to report

procedure Array_Index_Stmt is
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
begin
   V (3);
end Array_Index_Stmt;
