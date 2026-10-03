-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Each constrained object gets its own anonymous subtype and range.

-- CHECK-LABEL: ada.subp @object_constraint(
-- CHECK:         ada.range {{.*}} -> !ada.range<i32, @object_constraint.__anonymous_y>
-- CHECK:         ada.alloca : memref<!ada.qual<i32, @object_constraint.__anonymous_y>>
-- CHECK:         ada.range {{.*}} -> !ada.range<i32, @object_constraint.__anonymous_z>
-- CHECK:         ada.alloca : memref<!ada.qual<i32, @object_constraint.__anonymous_z>>
-- CHECK:         ada.decls {
-- CHECK-DAG:       ada.type @object_constraint.__anonymous_y base @standard.integer : i32 = #ada.int_info<range 1 to ?>
-- CHECK-DAG:       ada.type @object_constraint.__anonymous_z base @standard.integer : i32 = #ada.int_info<range 1 to ?>

procedure Object_Constraint (N : Integer) is
   Y, Z : Integer range 1 .. N;
begin
   null;
end Object_Constraint;
