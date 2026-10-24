-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir --mlir-print-debuginfo --mlir-print-local-scope %s | %FileCheck %s

-- CHECK-LABEL: ada.subp @loc(
-- CHECK-SAME:    %arg0: !ada.qual<i32, @standard.integer> loc("i"("{{.*}}loc.adb":16:15 to :16))
-- CHECK-SAME:    %arg1: !ada.qual<i32, @standard.integer> loc("j"("{{.*}}loc.adb":16:18 to :19))
-- CHECK-SAME:    %arg2: !ada.qual<i32, @standard.integer> loc("k"("{{.*}}loc.adb":16:21 to :22))
-- CHECK:         %{{.*}} = ada.binop "+" %arg0, %arg1 checks<overflow> : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer> loc("{{.*}}loc.adb":18:13 to :14)
-- CHECK:         %{{.*}} = ada.binop "+" %{{.*}}, %arg2 checks<overflow> : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer> loc("{{.*}}loc.adb":18:17 to :18)
-- CHECK:         ada.return %{{.*}} : !ada.qual<i32, @standard.integer> loc("{{.*}}loc.adb":18:4 to :21)
-- CHECK:       } loc("{{.*}}loc.adb":16:1 to 19:9)
-- CHECK:     } loc("{{.*}}loc.adb":16:1 to 19:9)

function Loc (I, J, K : Integer) return Integer is
begin
   return I + J + K;
end Loc;
