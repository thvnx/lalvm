-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- The module is stamped with an `llvm.ident` provenance string recording the
-- compiler identity: lalvm plus the LLVM version it was built against. It lands
-- in the object's `.comment` section, mirroring clang and GNAT.

-- CHECK: !llvm.ident = !{![[ID:[0-9]+]]}
-- CHECK: ![[ID]] = !{!"lalvm (LLVM {{.*}})"}

procedure Llvm_Ident is
begin
   null;
end Llvm_Ident;
