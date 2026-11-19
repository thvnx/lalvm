-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O0 --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -lm -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out
-- RUN: %lalvm -O1 --emit=obj %s -o %t.o
-- RUN: %cc %t.o %t.b.o %gnatstub -lm -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- Float conversions round ties away from zero (RM 4.6(33)).
-- A wrong result returns before the final overflow check.

procedure Conversion is
   type U8 is mod 256;
   type U32 is mod 2**32;
   function Id (X : Integer) return Integer
   is (X);
   N   : constant Integer := Id (10);
   subtype D is Integer range 1 .. N;
   F   : Float := 2.5;
   G   : Float := -F;
   H   : Float := 0.49999997;
   L   : Long_Float := 2147483647.4;
   UL  : Long_Float := 4294967295.0;
   Tie : Long_Float := 2147483647.5;
   U   : U8 := 255;
   I   : Integer := 5;
begin
   if Integer (F) /= 3 or Integer (G) /= -3 then
      return;
   end if;
   if Integer (H) /= 0 or Integer (L) /= Integer'Last then
      return;
   end if;
   if Integer (-Tie) /= Integer'First then
      return;
   end if;
   if U32 (UL) /= U32'Last or U8 (-H) /= 0 then
      return;
   end if;
   if Integer (-2.5) /= -3 then
      return;
   end if;
   if Float (I) /= 5.0 or Float (U) /= 255.0 then
      return;
   end if;
   -- A static operand is checked against the dynamic bounds of D (RM 4.6(28)).
   -- MLIR: ada.convert explicit %{{.*}}, %{{.*}} : !ada.qual<f128, @standard.universal_real_type_> to !ada.qual<i32, @conversion.d>
   -- MLIR: ada.convert explicit %{{.*}}, %{{.*}} : !ada.qual<f80, @standard.root_types_.root_real> to !ada.qual<i32, @conversion.d>
   if Integer (D (2.5)) /= 3 or Integer (D (5.0 / 2.0)) /= 3 then
      return;
   end if;
   -- CHECK: raised CONSTRAINT_ERROR : conversion.adb:[[#@LINE+1]] overflow check failed
   I := Integer (Tie);
end Conversion;
