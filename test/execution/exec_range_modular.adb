-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM
-- RUN: %lalvm --emit=obj %s -o %t.o
-- RUN: %lalvm --bind %s -o %t.b.o
-- RUN: %cc %t.o %t.b.o %gnatstub -o %t
-- RUN: %not %t 2> %t.out
-- RUN: %FileCheck %s < %t.out

-- A range check on a modular subtype compares unsigned (RM 3.5.4): 200 is in
-- Mid (100 .. 250) although negative as an i8, 251 is not..

-- LLVM-DAG: icmp ult i8 %{{.*}}, 100
-- LLVM-DAG: icmp ugt i8 %{{.*}}, -6

-- CHECK: raised CONSTRAINT_ERROR : exec_range_modular.adb:26 range check failed
procedure Exec_Range_Modular is
   type Byte is mod 256;
   subtype Mid is Byte range 100 .. 250;
   V : Byte := 200;
   X : Mid := V;
begin
   V := V + 51;
   X := V;
end Exec_Range_Modular;
