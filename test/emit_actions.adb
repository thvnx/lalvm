-- RUN: %lalvm --emit=ast  %s | %FileCheck %s --check-prefix=AST
-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
-- RUN: %lalvm --emit=asm  %s -o - | %FileCheck %s --check-prefix=ASM
-- RUN: %lalvm --emit=obj  %s -o %t.o
-- RUN: %llvm-dwarfdump --debug-info %t.o | %FileCheck %s --check-prefix=OBJ

-- Every --emit action for one subprogram: the AST dump, the Ada dialect MLIR,
-- the translated LLVM IR, and the two backend outputs lalvm produces itself
-- (target assembly, and an object file checked via its debug info).

-- AST: CompilationUnit

-- MLIR: ada.subp @emit_actions
-- MLIR: ada.constant : !ada.qual<i32, @standard.integer> = 0
-- MLIR: ada.return

-- LLVM: define i32 @_ada_emit_actions()
-- LLVM: ret i32 0

-- ASM: _ada_emit_actions

-- OBJ: DW_TAG_subprogram
-- OBJ: DW_AT_name ("emit_actions")

function Emit_Actions return Integer is
begin
   return 0;
end Emit_Actions;
