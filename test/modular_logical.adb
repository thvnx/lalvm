-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- Logical operators on modular types (RM 4.5.1), all stored in i8. They stay in
-- the storage width. `and`, and any operator on a power of two modulus, need no
-- reduction. `or` and `xor` on another modulus need one conditional `- m`.

-- Power of two modulus below 2**width: plain bitwise operation.

-- CHECK-LABEL: define internal i8 @modular_logical__and5(
-- CHECK:         and i8 %{{[0-9]+}}, %{{[0-9]+}}
-- CHECK-NEXT:    ret i8

-- CHECK-LABEL: define internal i8 @modular_logical__or5(
-- CHECK:         or i8 %{{[0-9]+}}, %{{[0-9]+}}
-- CHECK-NEXT:    ret i8

-- CHECK-LABEL: define internal i8 @modular_logical__xor5(
-- CHECK:         xor i8 %{{[0-9]+}}, %{{[0-9]+}}
-- CHECK-NEXT:    ret i8

-- Modulus of exactly 2**width: plain bitwise operation.

-- CHECK-LABEL: define internal i8 @modular_logical__xor256(
-- CHECK:         xor i8 %{{[0-9]+}}, %{{[0-9]+}}
-- CHECK-NEXT:    ret i8

-- Other modulus: `and` needs no reduction, `or` and `xor` need one.

-- CHECK-LABEL: define internal i8 @modular_logical__and10(
-- CHECK:         and i8 %{{[0-9]+}}, %{{[0-9]+}}
-- CHECK-NEXT:    ret i8

-- CHECK-LABEL: define internal i8 @modular_logical__or10(
-- CHECK-NOT:     zext
-- CHECK:         [[OR:%[0-9]+]] = or i8 %{{[0-9]+}}, %{{[0-9]+}}
-- CHECK-NEXT:    [[GE:%[0-9]+]] = icmp uge i8 [[OR]], 10
-- CHECK-NEXT:    [[SUB:%[0-9]+]] = sub i8 [[OR]], 10
-- CHECK-NEXT:    [[SEL:%[0-9]+]] = select i1 [[GE]], i8 [[SUB]], i8 [[OR]]
-- CHECK-NEXT:    ret i8 [[SEL]]

-- CHECK-LABEL: define internal i8 @modular_logical__xor10(
-- CHECK-NOT:     zext
-- CHECK:         [[XOR:%[0-9]+]] = xor i8 %{{[0-9]+}}, %{{[0-9]+}}
-- CHECK-NEXT:    [[GE:%[0-9]+]] = icmp uge i8 [[XOR]], 10
-- CHECK-NEXT:    [[SUB:%[0-9]+]] = sub i8 [[XOR]], 10
-- CHECK-NEXT:    [[SEL:%[0-9]+]] = select i1 [[GE]], i8 [[SUB]], i8 [[XOR]]
-- CHECK-NEXT:    ret i8 [[SEL]]

procedure Modular_Logical is
   type M5 is mod 2**5;
   type M10 is mod 10;
   type M256 is mod 256;

   function And5 (A, B : M5) return M5 is (A and B);
   function Or5 (A, B : M5) return M5 is (A or B);
   function Xor5 (A, B : M5) return M5 is (A xor B);
   function Xor256 (A, B : M256) return M256 is (A xor B);
   function And10 (A, B : M10) return M10 is (A and B);
   function Or10 (A, B : M10) return M10 is (A or B);
   function Xor10 (A, B : M10) return M10 is (A xor B);
begin
   null;
end Modular_Logical;
