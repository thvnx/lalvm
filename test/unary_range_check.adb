-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- A predefined unary operator yields the base type of its operand (RM 4.5.4,
-- 4.5.6), so assigning `abs Y` or `-Y` to an object of a constrained subtype
-- checks the subtype's range (RM 5.2, 4.6).

-- CHECK-LABEL: ada.subp @unary_range_check(
-- CHECK:         [[Y:%[0-9]+]] = memref.load %{{[0-9]+}}[] : memref<!ada.qual<i32, @unary_range_check.neg>>
-- CHECK-NEXT:    [[YB:%[0-9]+]] = ada.coerce [[Y]] : <i32, @unary_range_check.neg> to <i32, @standard.integer>
-- CHECK-NEXT:    [[ABS:%[0-9]+]] = ada.unop "abs" [[YB]] checks<overflow> : !ada.qual<i32, @standard.integer>
-- CHECK-NEXT:    [[C:%[0-9]+]] = ada.coerce [[ABS]] : <i32, @standard.integer> to <i32, @unary_range_check.neg>
-- CHECK:         ada.range_check [[C]]
-- CHECK:         [[Y2:%[0-9]+]] = memref.load %{{[0-9]+}}[] : memref<!ada.qual<i32, @unary_range_check.neg>>
-- CHECK-NEXT:    [[YB2:%[0-9]+]] = ada.coerce [[Y2]] : <i32, @unary_range_check.neg> to <i32, @standard.integer>
-- CHECK-NEXT:    [[NEG:%[0-9]+]] = ada.unop "-" [[YB2]] checks<overflow> : !ada.qual<i32, @standard.integer>
-- CHECK-NEXT:    [[C2:%[0-9]+]] = ada.coerce [[NEG]] : <i32, @standard.integer> to <i32, @unary_range_check.neg>
-- CHECK:         ada.range_check [[C2]]

procedure Unary_Range_Check is
   subtype Neg is Integer range -46 .. -8;
   Y : Neg := -12;
begin
   Y := abs Y;
   Y := -Y;
end Unary_Range_Check;
