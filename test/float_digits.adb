-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- The representation derives from the declared decimal precision: digits 4
-- fits single precision, digits 15 double, Long_Long_Float (digits 18) x86
-- extended.
-- MLIR: ada.type @standard.long_long_float : f80 = #ada.float_info<digits 18>
-- MLIR-LABEL: ada.subp @float_digits
-- MLIR: ada.constant : !ada.qual<f32, @float_digits.single>
-- MLIR: ada.constant : !ada.qual<f64, @float_digits.dbl>
-- MLIR: ada.constant : !ada.qual<f80, @standard.long_long_float>
-- MLIR: ada.type @float_digits.single : f32 = #ada.float_info<digits 4>
-- MLIR: ada.type @float_digits.dbl : f64 = #ada.float_info<digits 15>

-- LLVM-LABEL: define void @_ada_float_digits(

procedure Float_Digits is
   type Single is digits 4;
   type Dbl is digits 15;
   X : Single := 1.0;
   Y : Dbl := 2.0;
   Z : Long_Long_Float := 3.0;
begin
   null;
end Float_Digits;
