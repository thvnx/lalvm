-- RUN: %lalvm --emit=ast %s | %FileCheck %s

-- CHECK: CompilationUnit

function Emit_Ast return Integer is
begin
   return 0;
end Emit_Ast;
