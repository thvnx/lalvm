-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- A predefined unary operator yields the base type of its operand (RM 4.5.4,
-- 4.5.6), so assigning `abs Y` or `-Y` to an object of a constrained subtype
-- checks the subtype's range (RM 5.2, 4.6).

-- CHECK-LABEL: ada.subp @unary_range_check(
-- CHECK:         [[Y:%[0-9]+]] = memref.load %{{[0-9]+}}[] : memref<!ada.qual<i32, @unary_range_check.neg>>
-- CHECK-NEXT:    [[YB:%[0-9]+]] = ada.coerce [[Y]] : <i32, @unary_range_check.neg> to <i32, @standard.integer>
-- CHECK-NEXT:    [[ABS:%[0-9]+]] = ada.unop "abs" [[YB]] checks<overflow> : !ada.qual<i32, @standard.integer>
-- CHECK-NEXT:    ada.convert implicit [[ABS]] : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @unary_range_check.neg>
-- CHECK:         [[Y2:%[0-9]+]] = memref.load %{{[0-9]+}}[] : memref<!ada.qual<i32, @unary_range_check.neg>>
-- CHECK-NEXT:    [[YB2:%[0-9]+]] = ada.coerce [[Y2]] : <i32, @unary_range_check.neg> to <i32, @standard.integer>
-- CHECK-NEXT:    [[NEG:%[0-9]+]] = ada.unop "-" [[YB2]] checks<overflow> : !ada.qual<i32, @standard.integer>
-- CHECK-NEXT:    ada.convert implicit [[NEG]] : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @unary_range_check.neg>

-- LOWERED:       ada.unop "abs"
-- LOWERED:       ada.range_check %{{[0-9]+}}, %{{[0-9]+}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @unary_range_check.neg>
-- LOWERED:       ada.unop "-"
-- LOWERED:       ada.range_check %{{[0-9]+}}, %{{[0-9]+}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @unary_range_check.neg>

procedure Unary_Range_Check is
   subtype Neg is Integer range -46 .. -8;
   Y : Neg := -12;
begin
   Y := abs Y;
   Y := -Y;
end Unary_Range_Check;
