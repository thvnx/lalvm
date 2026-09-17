-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir -P %S/prj.gpr %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm -P %S/prj.gpr %s | %FileCheck %s --check-prefix=LLVM

-- MLIR:      module @package_body {
-- MLIR-NEXT:   ada.subp @package_body.initialize() {
-- MLIR:          ada.return
-- MLIR-NEXT:   }
-- MLIR-NEXT: }

-- LLVM:      define void @package_body__initialize() {
-- LLVM-NEXT:   ret void

package body Package_Body is

   procedure Initialize is
   begin
      null;
   end Initialize;

end Package_Body;
