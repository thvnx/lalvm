-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm -P %S/no_such_project.gpr --emit=mlir %s 2>&1 | %FileCheck %s

-- `-P` with a nonexistent project file fails at project loading time.

-- CHECK: lalvm: error: {{Libadalang raised|project: }}

procedure Invalid_Project is
begin
   null;
end Invalid_Project;
