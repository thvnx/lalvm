-- RUN: %lalvm -g --emit=mlir %s | %FileCheck %s

-- Under `-g`, `--emit=mlir` prints debug info (locations, local scope)
-- without the explicit `--mlir-print-debuginfo` flags.

-- CHECK: ada.subp @debug_mlir_dump
-- CHECK: loc(

function Debug_Mlir_Dump return Integer is
begin
   return 42;
end Debug_Mlir_Dump;
