-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=ast %s | %FileCheck %s --strict-whitespace

-- The tree is drawn in clang's -ast-dump style.

-- CHECK:      {{^}}CompilationUnit [
-- CHECK-NEXT: {{^}}`-LibraryItem [
-- CHECK:      {{^}}  `-SubpBody [
-- CHECK:      {{^}}    |-SubpSpec [
-- CHECK:      {{^}}    | `-SubtypeIndication [
-- CHECK:      {{^}}    |   `-Identifier "Integer" [
-- CHECK:      {{^}}    |-HandledStmts [
-- CHECK-NEXT: {{^}}    | `-StmtList [
-- CHECK-NEXT: {{^}}    |   `-ReturnStmt [
-- CHECK-NEXT: {{^}}    |     `-IntLiteral "0" [
-- CHECK-NEXT: {{^}}    `-EndName [
-- CHECK-NEXT: {{^}}      `-Identifier "Emit_Ast_Tree" [

function Emit_Ast_Tree return Integer is
begin
   return 0;
end Emit_Ast_Tree;
