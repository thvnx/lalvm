-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Preserve explicit modes, including each name in a grouped declaration.
-- CHECK-LABEL: ada.subp @param_explicit_mode(
-- CHECK-SAME: %a: !ada.qual<i32, @standard.integer>,
-- CHECK-SAME: %b: !ada.qual<i32, @standard.integer> {ada.mode = #ada<mode in>},
-- CHECK-SAME: %c: !ada.qual<i32, @standard.integer> {ada.mode = #ada<mode in>})

procedure Param_Explicit_Mode (A : Integer; B, C : in Integer) is
begin
   null;
end Param_Explicit_Mode;
