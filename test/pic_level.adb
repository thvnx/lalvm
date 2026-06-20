-- RUN: %lalvm --emit=llvm %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm --relocation-model=static %s \
-- RUN:   | %FileCheck %s --check-prefix=STATIC

-- The default relocation model is PIC, recorded as a "PIC Level" module flag so
-- the IR matches the relocation model. An explicit non-PIC model omits it.

-- CHECK: !"PIC Level", i32 2

-- STATIC-NOT: PIC Level

procedure Pic_Level is
begin
   null;
end Pic_Level;
