-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir -P %S/prj.gpr %s 2>&1 | %FileCheck %s

-- Check that a body whose spec is not in the project is rejected.

-- CHECK: pkg.adb:10:14: error: specification of package Pkg not found

package body Pkg is

   procedure Proc is
   begin
      null;
   end Proc;

end Pkg;
