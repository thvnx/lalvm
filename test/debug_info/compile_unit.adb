-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm -g --emit=llvm %s | %FileCheck %s

-- CHECK: !DICompileUnit(language: DW_LANG_Ada2012
-- CHECK-SAME:           producer: "lalvm (LLVM {{.*}}, Libadalang {{.*}})"

procedure Compile_Unit is
begin
   null;
end Compile_Unit;
