-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- `S'Last` and predefined operators yield the type of `S`, not the subtype `S`:
-- `-Small'Last` folds to -30 of `Integer`, outside `Small` but valid for `X`,
-- so no warning and no range check.

-- CHECK-NOT:     warning
-- CHECK-LABEL: ada.subp @scalar_attr_base_type
-- CHECK-NOT:     ada.range_check
-- CHECK:         ada.constant : !ada.qual<i32, @standard.integer> = -30
-- CHECK-NOT:     ada.range_check
-- CHECK:         ada.constant : !ada.qual<i32, @standard.integer> = 30
-- CHECK-NOT:     ada.range_check
-- CHECK:         ada.return

function Scalar_Attr_Base_Type return Integer is
   subtype Small is Integer range 1 .. 30;
   X : Integer;
   Y : Integer;
begin
   X := -Small'Last;
   Y := Small'Last;
   return X + Y;
end Scalar_Attr_Base_Type;
