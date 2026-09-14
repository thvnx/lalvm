-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --bind --emit=llvm %s 2>&1 | %FileCheck %s

-- CHECK: bind_params.adb:8:24: error: the main subprogram cannot have parameters

procedure Bind_Params (X : Integer) is
begin
   null;
end Bind_Params;
