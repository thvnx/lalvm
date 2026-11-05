-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- The type carries the flag, and its subtype inherits it through the base link.
-- CHECK: ada.type @type_default_value.t : i{{[0-9]+}} = {{.*}} {has_default_value}
-- CHECK: ada.type @type_default_value.s base @type_default_value.t : i{{[0-9]+}}

procedure Type_Default_Value is
   type T is range 0 .. 100 with Default_Value => 7;
   subtype S is T;
begin
   null;
end Type_Default_Value;
