-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Boolean logical operators on a Boolean subtype (RM 4.5.1). `Flag` adds no
-- constraint, so it is a pure renaming of Boolean (RM 3.2.2): the operands take
-- the base type and 'and'/'or'/'xor' resolve to the predefined Boolean
-- operators (result `@standard.boolean`), lowering to the bitwise i1 ops.
-- Operands are nested parameters so the ops are not folded.

-- MLIR: ada.binop "and" %{{.*}}, %{{.*}} : !ada.qual<i1, @standard.boolean>
-- MLIR: ada.binop "or" %{{.*}}, %{{.*}} : !ada.qual<i1, @standard.boolean>
-- MLIR: ada.binop "xor" %{{.*}}, %{{.*}} : !ada.qual<i1, @standard.boolean>

-- LLVM-DAG: and i1
-- LLVM-DAG: or i1
-- LLVM-DAG: xor i1

function Logical_Subtype return Boolean is
   subtype Flag is Boolean;
   function Combine (X, Y : Flag) return Boolean is
      A : Boolean := X and Y;
      B : Boolean := X or Y;
      C : Boolean := X xor Y;
   begin
      return A;
   end Combine;
begin
   return Combine (True, False);
end Logical_Subtype;
