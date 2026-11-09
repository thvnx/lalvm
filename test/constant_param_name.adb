-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O0 -g --emit=llvm %s | %FileCheck %s --check-prefix=DI

-- A constant view of a parameter must preserve the parameter's source name.
-- MLIR-LABEL: ada.subp @constant_param_name(%n:
-- MLIR: ada.return %n
-- DI: DILocalVariable(name: "n", arg: 1,

function Constant_Param_Name (N : Integer) return Integer is
   Copy : constant Integer := N;
begin
   return Copy;
end Constant_Param_Name;
