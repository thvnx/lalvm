-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s
-- CHECK: error: array extent exceeds the layout limit

procedure Array_Extent_Overflow is
   type Vec is array (Long_Long_Integer'(0) .. Long_Long_Integer'Last) of Integer;
begin
   null;
end Array_Extent_Overflow;
