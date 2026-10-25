-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir --mlir-print-debuginfo --mlir-print-local-scope %s | %FileCheck %s

-- The implicit return of a procedure is located at its end name.

-- CHECK-LABEL: ada.subp @implicit_return_loc(
-- CHECK:         ada.return loc("{{.*}}implicit_return_loc.adb":14:5 to :24)

procedure Implicit_Return_Loc (X : in out Integer) is
begin
   X := X + 1;
end Implicit_Return_Loc;
