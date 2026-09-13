-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=obj %s -o %t.o 2>&1 | %FileCheck %s
-- RUN: %not %lalvm --emit=asm %s -o %t.s 2>&1 | %FileCheck %s

-- CHECK: cannot generate code for file {{.*}}spec_no_code.ads (package spec)

package Spec_No_Code is

   procedure Initialize;

end Spec_No_Code;
