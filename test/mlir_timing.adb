-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=llvm -O1 --mlir-timing %s -o /dev/null 2>&1 | %FileCheck %s

-- CHECK: Execution time report
-- CHECK: ClosureConversionPass
-- CHECK: Mem2Reg
-- CHECK: AdaToLLVMLoweringPass

procedure Mlir_Timing is
begin
   null;
end Mlir_Timing;
