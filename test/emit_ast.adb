-- RUN: %lalvm --emit=ast %s | %FileCheck %s

-- CHECK: CompilationUnit

function Test_Emit_Ast return Integer is
begin
   return 0;
end Test_Emit_Ast;
