-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: rm -rf %t.dir && mkdir -p %t.dir && cd %t.dir && %lalvm --bind %s && test -f b_bind_default_output.o
-- RUN: cd %t.dir && %lalvm --bind --emit=asm %s && test -f b_bind_default_output.s

-- Check that bind objects start with `_b` prefix.

procedure Bind_Default_Output is
begin
   null;
end Bind_Default_Output;
