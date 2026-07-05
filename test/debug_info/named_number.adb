-- RUN: %lalvm -O1 --emit=mlir --mlir-print-debuginfo --mlir-print-local-scope %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm -O1 --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Named number declaration (RM 3.3.2) emits an ada.constant with NameLoc
-- at universal type for DWARF. No alloca is ever created, so AdaDebugInfoPass
-- emits a dbg.value intrinsic (not dbg.declare).

-- MLIR-LABEL: ada.subp @named_number
-- MLIR-SAME:    () -> !ada.qual<i32, @standard.integer>
-- MLIR:         ada.constant : !ada.qual<i512, @standard.universal_int_type_> = 200 loc("max"(
-- MLIR:         %[[MAX:.*]] = ada.coerce {{.*}} : <i512, @standard.universal_int_type_> to <i32, @standard.integer>
-- MLIR:         ada.return %[[MAX]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_named_number(
-- LLVM:          #dbg_value(i512 200,
-- LLVM:          DILocalVariable(name: "max",

function Named_Number return Integer is
   Max : constant := 200;
begin
   return Max;
end Named_Number;
