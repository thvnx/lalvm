-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm -O4 --emit=mlir %s 2>&1 | %FileCheck %s

-- CHECK: for the -O option: optimization level 4 not in range 0 .. 3

procedure Opt_Level_Invalid is
begin
   null;
end Opt_Level_Invalid;
