-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --bind --emit=llvm %s 2>&1 | %FileCheck %s

-- CHECK: bind_spec.ads:{{.*}}: error: cannot bind a subprogram spec: the main unit must be a subprogram body

procedure Bind_Spec;
