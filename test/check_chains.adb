-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- A checked operation feeding another check keeps both checks, and the second
-- one consumes the result of the first: MLIRGen converts the result, and
-- LowerAdaConversions checks it.

-- LOWERED-LABEL: func.func @_ada_check_chains(
-- LOWERED:         %[[Q1:.*]] = ada.binop "/" %{{.*}}, %{{.*}} checks<division>
-- LOWERED:         ada.binop "+" %[[Q1]], %{{.*}} checks<overflow>
-- LOWERED:         %[[Q2:.*]] = ada.binop "/" %{{.*}}, %{{.*}} checks<division>
-- LOWERED:         ada.range_check %[[Q2]], %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @check_chains.small>
-- LOWERED:         %[[NEXT:.*]] = ada.binop "+" %{{.*}}, %{{.*}} checks<overflow>
-- LOWERED:         ada.index_check %[[NEXT]], %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @check_chains.__anonymous_vec_index_1>
-- LOWERED-LABEL: func.func private @check_chains__succ(
-- LOWERED:         %[[SUM:.*]] = ada.binop "+" %{{.*}}, %{{.*}} checks<overflow>
-- LOWERED:         %[[CHK:.*]] = ada.range_check %[[SUM]], %{{.*}} : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @check_chains.small>
-- LOWERED:         return %[[CHK]] : !ada.qual<i32, @check_chains.small>

procedure Check_Chains (A, B, C, I : Integer) is
   subtype Small is Integer range 1 .. 10;
   type Vec is array (1 .. 10) of Integer;
   V : Vec;
   R : Integer;
   S : Small;

   -- CHECK-LABEL: ada.subp private @check_chains.succ
   -- CHECK:         %[[SUM:.*]] = ada.binop "+" %{{.*}}, %{{.*}} checks<overflow>
   -- CHECK:         %[[RES:.*]] = ada.convert implicit %[[SUM]] : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @check_chains.small>
   -- CHECK:         ada.return %[[RES]]
   function Succ (X : Integer) return Small is
   begin
      return X + 1;
   end Succ;
begin
   -- CHECK:       %[[Q1:.*]] = ada.binop "/" %{{.*}}, %{{.*}} checks<division>
   -- CHECK:       ada.binop "+" %[[Q1]], %{{.*}} checks<overflow>
   R := (A / B) + C;

   -- CHECK:       %[[Q2:.*]] = ada.binop "/" %{{.*}}, %{{.*}} checks<division>
   -- CHECK:       ada.convert implicit %[[Q2]] : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @check_chains.small>
   S := A / B;

   -- CHECK:       %[[NEXT:.*]] = ada.binop "+" %{{.*}}, %{{.*}} checks<overflow>
   -- CHECK:       ada.index %{{.*}}[%[[NEXT]]]
   R := V (I + 1);

   R := Succ (S);
end Check_Chains;
