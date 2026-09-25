-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- Check that llvm.ident contains lalvm's version.

-- CHECK: !llvm.ident = !{![[ID:[0-9]+]]}
-- CHECK: ![[ID]] = !{!"lalvm (LLVM {{.*}}, Libadalang {{.+}})"}

procedure LLVM_Ident is
begin
   null;
end LLVM_Ident;
