-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- CHECK: module @package_parts {
-- CHECK-NEXT: }

package Package_Parts is

   procedure Initialize;

private

   procedure Reset;

end Package_Parts;
