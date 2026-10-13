-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Relational operators compare on the base type (RM 4.5.2, RM 3.5).

-- CHECK-LABEL: ada.subp @relation_subtype
-- CHECK:         ada.decls
-- CHECK-NOT:     ada.range_check
-- CHECK:         %[[Y:.*]] = memref.load %{{.*}} : memref<!ada.qual<i32, @relation_subtype.small>>
-- CHECK:         %[[I:.*]] = memref.load %{{.*}} : memref<!ada.qual<i32, @standard.integer>>
-- CHECK:         %[[YB:.*]] = ada.coerce %[[Y]] : <i32, @relation_subtype.small> to <i32, @standard.integer>
-- CHECK:         ada.cmp "=" %[[YB]], %[[I]] : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>)
-- CHECK-NOT:     ada.range_check
-- CHECK:         ada.constant : !ada.qual<i32, {{.*}}> = 111
-- CHECK:         ada.cmp "<=" %{{.*}}, %{{.*}} : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>)
-- CHECK-NOT:     ada.range_check
-- CHECK:         %[[Y2:.*]] = memref.load %{{.*}} : memref<!ada.qual<i32, @relation_subtype.small>>
-- CHECK:         %[[C:.*]] = ada.constant : !ada.qual<i32, @standard.integer> = 200
-- CHECK:         %[[Y2B:.*]] = ada.coerce %[[Y2]] : <i32, @relation_subtype.small> to <i32, @standard.integer>
-- CHECK:         ada.cmp ">=" %[[Y2B]], %[[C]] : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>)
-- CHECK-NOT:     ada.range_check
-- CHECK:         %[[Z:.*]] = memref.load %{{.*}} : memref<!ada.qual<i32, @relation_subtype.tiny>>
-- CHECK:         %[[I2:.*]] = memref.load %{{.*}} : memref<!ada.qual<i32, @standard.integer>>
-- CHECK:         %[[ZB:.*]] = ada.coerce %[[Z]] : <i32, @relation_subtype.tiny> to <i32, @standard.integer>
-- CHECK:         ada.cmp "=" %[[ZB]], %[[I2]] : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>)
-- CHECK-NOT:     ada.range_check
-- CHECK:         %[[W:.*]] = memref.load %{{.*}} : memref<!ada.qual<i3, @relation_subtype.warm>>
-- CHECK:         %[[CL:.*]] = memref.load %{{.*}} : memref<!ada.qual<i3, @relation_subtype.color>>
-- CHECK:         %[[WB:.*]] = ada.coerce %[[W]] : <i3, @relation_subtype.warm> to <i3, @relation_subtype.color>
-- CHECK:         ada.cmp "<" %[[WB]], %[[CL]] : (!ada.qual<i3, @relation_subtype.color>, !ada.qual<i3, @relation_subtype.color>)
-- CHECK-NOT:     ada.range_check
-- CHECK:         ada.return

procedure Relation_Subtype is
   subtype Small is Integer range 4 .. 111;
   subtype Tiny is Small range 5 .. 10;
   type Color is (Red, Green, Blue);
   subtype Warm is Color range Red .. Green;
   Y  : Small := 25;
   Z  : Tiny := 7;
   W  : Warm := Green;
   C  : Color := Blue;
   I  : Integer := -1;
   B1 : Boolean;
   B2 : Boolean;
   B3 : Boolean;
   B4 : Boolean;
   B5 : Boolean;
begin
   B1 := Y = I;
   B2 := Small'Last <= I;
   B3 := Y >= 200;
   B4 := Z = I;
   B5 := W < C;
end Relation_Subtype;
