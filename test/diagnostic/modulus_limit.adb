-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=llvm %s 2>&1 | %FileCheck %s

-- A binary modulus is at most System.Max_Binary_Modulus (RM 3.5.4).

-- CHECK: modulus_limit.adb:11:21: error: modulus exceeds limit (2 ** 128)

procedure Modulus_Limit is
   type Huge is mod 2 ** 3131;
begin
   null;
end Modulus_Limit;
