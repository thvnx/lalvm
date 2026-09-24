-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- A null procedure is a body with only an implicit return.

-- MLIR-LABEL: ada.subp @null_procedure() {
-- MLIR:         ada.decls {
-- MLIR-NEXT:      ada.subp private @null_procedure.p() {
-- MLIR-NEXT:        ada.return
-- MLIR-NEXT:      }

-- LLVM:      define internal void @null_procedure__p() {
-- LLVM-NEXT:   ret void

procedure Null_Procedure is
   procedure P is null;
begin
   null;
end Null_Procedure;
