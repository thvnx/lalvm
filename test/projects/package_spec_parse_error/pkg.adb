-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir -P %S/prj.gpr %s 2>&1 | %FileCheck %s

-- Check that a parse error in the spec is reported and no code is generated for
-- the body.

-- CHECK: pkg.ads:5:{{[0-9]+}}: error:
-- CHECK-NOT: module @pkg

package body Pkg is

   procedure Private_Body (X : T) is
   begin
      null;
   end Private_Body;

end Pkg;
