-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --bind --emit=llvm %s 2>&1 | %FileCheck %s

-- LALVM only binds a main function returning Integer, as per RM 10.2(29).

-- CHECK: bind_result_long_integer.adb:10:{{[0-9]+}}: error: the main function must return Integer

function Bind_Result_Long_Integer return Long_Integer is
begin
   return 0;
end Bind_Result_Long_Integer;
