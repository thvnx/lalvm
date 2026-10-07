-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=llvm %s 2>&1 | %FileCheck %s

-- A modulus is positive (RM 3.5.4).

-- CHECK: modulus_not_positive.adb:11:21: error: modulus value must be positive

procedure Modulus_Not_Positive is
   type None is mod 0;
begin
   null;
end Modulus_Not_Positive;
