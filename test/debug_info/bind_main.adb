-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --bind -g --emit=llvm %s | %FileCheck %s

-- CHECK: define i32 @main({{.*}}) !dbg [[SP:![0-9]+]]
-- CHECK: !DICompileUnit(language: DW_LANG_Ada2012
-- CHECK: [[SP]] = distinct !DISubprogram(name: "main"

procedure Bind_Main is
begin
   null;
end Bind_Main;
