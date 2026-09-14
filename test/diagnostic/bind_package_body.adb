-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --bind --emit=llvm %s 2>&1 | %FileCheck %s

-- CHECK: bind_package_body.adb:8:{{[0-9]+}}: error: the main unit must be a subprogram body

package body Bind_Package_Body is
end Bind_Package_Body;
