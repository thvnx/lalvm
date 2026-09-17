-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir -P %S/prj.gpr %s | %FileCheck %s

-- A package body is compiled with its spec: the spec's declarations first, then
-- the body's, all scoped by the package name.

-- CHECK-LABEL: module @pkg {
-- CHECK-NEXT:    ada.type @pkg.public_t : i1 = #ada.enum_info<"vrai" = 0, "faux" = 1>
-- CHECK-NEXT:    ada.type @pkg.private_t : i1 = #ada.enum_info<"vert" = 0, "rouge" = 1>
-- CHECK-NEXT:    ada.subp @pkg.public_proc() {
-- CHECK:           ada.return
-- CHECK-NEXT:    }
-- CHECK-NEXT:    ada.subp @pkg.private_body(%arg0: !ada.qual<i1, @pkg.public_t>, %arg1: !ada.qual<i1, @pkg.private_t>) {
-- CHECK:           ada.return
-- CHECK-NEXT:    }
-- CHECK-NEXT:  }

package body Pkg is

   procedure Public_Proc is
   begin
      null;
   end Public_Proc;

   procedure Private_Body (X : Public_T; Y : Private_T) is
   begin
      null;
   end Private_Body;

end Pkg;
