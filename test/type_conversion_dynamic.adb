-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=LOWERED

-- Explicit type conversion to a dynamic subtype: checked against the range
-- elaborated at the subtype declaration.

-- CHECK:      %[[R:.*]] = ada.range %{{.*}}, %arg0 : !ada.qual<i32, @standard.integer> -> !ada.range<i32, @type_conversion_dynamic.s>
-- CHECK:      ada.convert explicit %arg1, %[[R]] : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @type_conversion_dynamic.s>

-- LOWERED:    %[[R:.*]] = ada.range %{{.*}}, %arg0
-- LOWERED:    ada.range_check %arg1, %[[R]] : !ada.qual<i32, @standard.integer> to !ada.qual<i32, @type_conversion_dynamic.s>

function Type_Conversion_Dynamic (N, X : Integer) return Integer is
   subtype S is Integer range 1 .. N;
begin
   return S (X);
end Type_Conversion_Dynamic;
