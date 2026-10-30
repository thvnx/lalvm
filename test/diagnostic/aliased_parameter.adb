-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s
-- CHECK: error: explicitly aliased parameters are not supported

procedure Aliased_Parameter (X : aliased in out Integer) is
begin
   X := X + 1;
end Aliased_Parameter;
