-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: rm -rf %t.dir && mkdir -p %t.dir && cd %t.dir && %lalvm --emit=asm %s && test -f default_output_asm.s
-- RUN: cd %t.dir && %lalvm --emit=obj - < %s && test -f a.o

-- Without `-o`, the output is `<stem>.s` in the current directory. The stem of
-- the standard input (`-`) is `a`.

function Default_Output_Asm return Integer is
begin
   return 0;
end Default_Output_Asm;
