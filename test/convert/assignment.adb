-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Assignment.

-- CHECK: %[[C:.*]] = ada.coerce %arg0 : <i32, @standard.integer> to <i32, @assignment.small>
-- CHECK: ada.range_check %[[C]]

procedure Assignment (X : Integer) is
   subtype Small is Integer range 1 .. 10;
   Y : Small;
begin
   Y := X;
end Assignment;
