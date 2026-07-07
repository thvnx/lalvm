-- RUN: %lalvm -g --emit=llvm %s | %FileCheck %s --check-prefix=O0 --implicit-check-not='DILocalVariable(name: "true"'
-- RUN: %lalvm -O1 -g --emit=llvm %s | %FileCheck %s --check-prefix=O1 --implicit-check-not='DILocalVariable(name: "true"'

-- A variable initialized from an enum literal must not be described by a
-- DILocalVariable named after the literal ("true"). At -O0 B keeps its own name
-- via the alloca; at -O1 it is promoted to the literal constant, which carries
-- no NameLoc, so no "true"-named variable appears.

-- O0: !DILocalVariable(name: "b"
-- O1-LABEL: define {{.*}}@_ada_enum_lit_no_shadow

procedure Enum_Lit_No_Shadow is
   B : Boolean := True;
begin
   null;
end Enum_Lit_No_Shadow;
