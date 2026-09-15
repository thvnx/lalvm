-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- CHECK:      module @package_body {
-- CHECK-NEXT:   ada.subp @initialize() {
-- CHECK:          ada.return
-- CHECK-NEXT:   }
-- CHECK-NEXT: }

package body Package_Body is

   procedure Initialize is
   begin
      null;
   end Initialize;

end Package_Body;
