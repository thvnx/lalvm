-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %check-exit %t 1 2>&1 | %FileCheck %s

-- 'Pos returns universal_integer. The Boolean condition supplies no concrete
-- integer type, so Libadalang selects root_integer for the arithmetic.
-- MLIR: ada.type @standard.root_types_.root_integer : i128
-- MLIR: ada.binop "+" %{{.*}}, %{{.*}} checks<overflow> : (!ada.qual<i128, @standard.root_types_.root_integer>, !ada.qual<i128, @standard.root_types_.root_integer>) -> !ada.qual<i128, @standard.root_types_.root_integer>

function Universal_Integer_Overflow return Integer is
   X : Long_Long_Long_Integer := Long_Long_Long_Integer'Last;
begin
   -- CHECK: raised CONSTRAINT_ERROR : universal_integer_overflow.adb:[[#@LINE+1]] overflow check failed
   if Long_Long_Long_Integer'Pos (X) + 1 > 0 then
      return 1;
   end if;
   return 0;
end Universal_Integer_Overflow;
