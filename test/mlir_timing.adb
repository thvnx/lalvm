-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=llvm -O1 --mlir-timing %s -o /dev/null 2>&1 | %FileCheck %s

-- CHECK: Execution time report
-- CHECK: ClosureConversion
-- CHECK: Mem2Reg
-- CHECK: LowerToLLVM

procedure Mlir_Timing is
begin
   null;
end Mlir_Timing;
