-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Named numbers and static comparisons remain exact beyond the runtime width
-- (RM 4.9). The final value is 2**70.
-- CHECK: ada.constant : !ada.qual<i128, @standard.universal_int_type_> = 1180591620717411303424
-- CHECK: ada.constant : !ada.qual<i1, @standard.boolean> = false
-- CHECK: ada.constant : !ada.qual<i1, @standard.boolean> = true

-- LLVM: ret i128 1180591620717411303424

function Number_Decl_Wide return Long_Long_Long_Integer is
   type Word is mod 2**128;
   Huge : constant := 2**600;
   High : constant := 2**128 - 1;
   Big  : constant := Huge + 1180591620717411303424 - Huge;
   U    : Word := High;
begin
   if U /= Word'Last or Huge = 0 then
      return 0;
   end if;
   if Huge + 1 > Huge then
      return Big;
   end if;
   return 0;
end Number_Decl_Wide;
