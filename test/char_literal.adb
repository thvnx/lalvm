-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- A character literal denotes an enumeration literal of the predefined type
-- Standard.Character (RM 3.5.2), a Latin-1 enumeration lowered to an i8. 'A'
-- has code point 65. Libadalang materializes only the referenced literals, so
-- the emitted ada.type lists just 'A' (with its code point, not its position).

-- MLIR-LABEL: ada.subp @char_literal
-- MLIR:         %[[C:.*]] = ada.constant : !ada.qual<i8, @standard.character> = 65
-- MLIR-NEXT:    ada.return %[[C]] : !ada.qual<i8, @standard.character>

-- LLVM-LABEL: define i8 @_ada_char_literal()
-- LLVM:         ret i8 65

function Char_Literal return Character is
begin
   return 'A';
end Char_Literal;
