-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Modular subtype operand, to its base.

-- CHECK: ada.coerce %{{.*}} : <i8, @modular_operand.low> to <i8, @modular_operand.byte>

function Modular_Operand return Boolean is
   type Byte is mod 256;
   subtype Low is Byte range 0 .. 15;
   function Add (X : Low; Y : Byte) return Byte is
   begin
      return X + Y;
   end Add;
begin
   return Add (1, 2) = 3;
end Modular_Operand;
