-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: rm -rf %t.dir && mkdir -p %t.dir
-- RUN: cd %t.dir && %lalvm --emit=asm %s && test -f default_output.s
-- RUN: cd %t.dir && %lalvm --emit=obj %s && test -f default_output.o
-- RUN: cd %t.dir && %lalvm --emit=obj - < %s && test -f a.o

function Default_Output return Integer is
begin
   return 0;
end Default_Output;
