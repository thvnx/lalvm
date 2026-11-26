-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- Numeric conversions check the rounded float's base range before casting,
-- then the subtype range. Static conversions are folded (RM 4.6).

-- CHECK-LABEL: ada.subp @type_conversion
-- CHECK:       ada.convert explicit %{{.*}} : !ada.qual<f32, @standard.float> to !ada.qual<i32, @standard.integer>
-- CHECK:       ada.convert explicit %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<f32, @standard.float>
-- CHECK-NEXT:  ada.write
-- CHECK-NEXT:  ada.constant : !ada.qual<i32, @standard.integer> = 3
-- CHECK-NEXT:  ada.write
-- CHECK:       ada.constant : !ada.qual<f32, @standard.float> = 0x4B800000

-- LOWERED-LABEL: func.func @_ada_type_conversion
-- LOWERED:       %[[F:.*]] = ada.read %{{.*}} : !ada.ref<!ada.qual<f32, @standard.float>> -> !ada.qual<f32, @standard.float>
-- LOWERED-NEXT:  %[[RAW:.*]] = ada.unwrap %[[F]]
-- LOWERED-NEXT:  %[[ROUNDED_FLOAT:.*]] = math.round %[[RAW]] : f32
-- LOWERED-NEXT:  %[[WRAPPED:.*]] = ada.wrap %[[ROUNDED_FLOAT]]
-- LOWERED-NEXT:  %[[LO:.*]] = ada.constant : !ada.qual<f32, @standard.float> = -2.14748365E+9
-- LOWERED-NEXT:  %[[HI:.*]] = ada.constant : !ada.qual<f32, @standard.float> = 2.14748352E+9
-- LOWERED-NEXT:  %[[R:.*]] = ada.range %[[LO]], %[[HI]]
-- LOWERED-NEXT:  %[[C:.*]] = ada.range_check %[[WRAPPED]], %[[R]] : !ada.qual<f32, @standard.float> to !ada.qual<f32, @standard.float>
-- LOWERED-NEXT:  %[[U:.*]] = ada.unwrap %[[C]]
-- LOWERED-NEXT:  %[[I:.*]] = arith.fptosi %[[U]] : f32 to i32
-- LOWERED-NEXT:  ada.wrap %[[I]] : i32 to !ada.qual<i32, @standard.integer>
-- LOWERED:       %[[X:.*]] = ada.unwrap %{{.*}} : !ada.qual<i32, @standard.integer> to i32
-- LOWERED-NEXT:  %[[Y:.*]] = arith.sitofp %[[X]] : i32 to f32
-- LOWERED-NEXT:  ada.wrap %[[Y]] : f32 to !ada.qual<f32, @standard.float>

-- A float converted to Small checks the integer base range, then Small.
-- LOWERED:       ada.range_check %{{.*}}, %{{.*}} : !ada.qual<f32, @standard.float> to !ada.qual<f32, @standard.float>
-- LOWERED:       %[[ROUNDED:.*]] = arith.fptosi %{{.*}} : f32 to i32
-- LOWERED-NEXT:  %[[BASE:.*]] = ada.wrap %[[ROUNDED]] : i32 to !ada.qual<i32, @standard.integer>
-- LOWERED:       ada.range_check %[[BASE]], %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @type_conversion.small>

-- LOWERED:       return

procedure Type_Conversion is
   subtype Small is Integer range 1 .. 10;
   I : Integer := 1;
   F : Float := 2.5;
   Y : Small := 3;
begin
   I := Integer (F);
   F := Float (I);
   I := Integer (2.5);
   Y := Small (F);
   F := Float (16_777_217);
end Type_Conversion;
