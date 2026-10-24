-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm -g --emit=mlir %s | %FileCheck %s
-- RUN: %lalvm -g --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=DUMP
-- RUN: %lalvm -g --mlir-print-debuginfo=false --emit=llvm --mlir-print-ir-after=ada-lower-conversions %s -o /dev/null 2>&1 | %FileCheck %s --check-prefix=NODEBUG

-- Under `-g`, `--emit=mlir` and the IR dumps print debug info (locations,
-- local scope) without the explicit `--mlir-print-debuginfo` flags.

-- CHECK: ada.subp @debug_mlir_dump
-- CHECK: loc(

-- DUMP: IR Dump After LowerAdaConversions
-- DUMP: ada.subp @debug_mlir_dump
-- DUMP: loc(

-- NODEBUG: IR Dump After LowerAdaConversions
-- NODEBUG-NOT: loc(

function Debug_Mlir_Dump return Integer is
begin
   return 42;
end Debug_Mlir_Dump;
