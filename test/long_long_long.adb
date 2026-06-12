-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Long_Long_Long_Integer is 128-bit in GNAT on 64-bit targets; its bounds
-- exceed int64 and exercise the APInt-based width derivation.
-- MLIR: ada.type @standard.long_long_long_integer : i128 = #ada.int_info<range -170141183460469231731687303715884105728 to 170141183460469231731687303715884105727>
-- MLIR-LABEL: ada.subp @long_long_long
-- MLIR: ada.constant : !ada.qual<i128, @standard.long_long_long_integer> = 42

-- LLVM-LABEL: define i128 @_ada_long_long_long(
-- LLVM: ret i128 42

function Long_Long_Long return Long_Long_Long_Integer is
begin
   return 42;
end Long_Long_Long;
