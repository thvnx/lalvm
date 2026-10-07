-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Modular representation width (RM 3.5.4): 0 .. modulus-1 byte-rounded to a
-- power of two, so `mod 100_000` needs 17 bits and gets i32, and a 33..64-bit
-- modulus gets i64. The largest supported moduli, 2**128 and 2**32 - 1, compile.

-- CHECK-DAG: ada.type @modular_width.m32 : i32 = #ada.int_info<mod 100000{{.*}}>
-- CHECK-DAG: ada.type @modular_width.m64 : i64 = #ada.int_info<mod 1099511627776{{.*}}>
-- CHECK-DAG: ada.type @modular_width.max_binary : i128 = #ada.int_info<mod 340282366920938463463374607431768211456{{.*}}>
-- CHECK-DAG: ada.type @modular_width.max_nonbinary : i32 = #ada.int_info<mod 4294967295{{.*}}>

procedure Modular_Width is
   type M32 is mod 100_000;
   type M64 is mod 2**40;
   type Max_Binary is mod 2**128;
   type Max_Nonbinary is mod 2**32 - 1;
begin
   null;
end Modular_Width;
