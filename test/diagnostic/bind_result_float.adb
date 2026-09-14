-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --bind --emit=llvm %s 2>&1 | %FileCheck %s

-- CHECK: bind_result_float.adb:8:{{[0-9]+}}: error: the main function must return Integer

function Bind_Result_Float return Float is
begin
   return 0.0;
end Bind_Result_Float;
