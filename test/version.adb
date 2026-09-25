-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --version | %FileCheck %s

-- CHECK: lalvm (LLVM {{[0-9]+\.[0-9]+.*}}, Libadalang {{.+}})

procedure Version is
begin
   null;
end Version;
