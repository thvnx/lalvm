-- RUN: %lalvm --emit=mlir --mlir-print-debuginfo --mlir-print-local-scope %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Named number declaration (RM 3.3.2) emits an arith.constant with NameLoc
-- at universal type for DWARF. No alloca is ever created, so Mem2Reg has
-- nothing to promote and FinalizeAdaObject turns the constant into a
-- dbg.value intrinsic (not dbg.declare).

-- MLIR-LABEL: ada.subp @test_named_number_dbg
-- MLIR-SAME:    () -> i32
-- MLIR:         arith.constant {ada.type = @standard.universal_int_type_} 200 : i64 loc("max"(
-- MLIR:         %[[MAX:.*]] = arith.constant {ada.type = @standard.integer} 200 : i32
-- MLIR:         ada.return %[[MAX]] : i32

-- LLVM-LABEL: define i32 @_ada_test_named_number_dbg(
-- LLVM:          #dbg_value(i64 200,
-- LLVM:          DILocalVariable(name: "max",

function Test_Named_Number_Dbg return Integer is
   Max : constant := 200;
begin
   return Max;
end Test_Named_Number_Dbg;
