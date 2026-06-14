-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Signed-integer `+`/`-`/`*` carry `checks<overflow>` (RM 4.5, 3.5.4); modular
-- arithmetic wraps and floats are deferred, so neither is flagged. The nested
-- modular and float ops are emitted before the outer signed one.
-- CHECK:      ada.binop "+" %{{.*}}, %{{.*}} : !ada.qual<i8, @binop_overflow.byte>
-- CHECK:      ada.binop "*" %{{.*}}, %{{.*}} : !ada.qual<f32, @standard.float>
-- CHECK:      ada.binop "+" %{{.*}}, %{{.*}} checks<overflow> : !ada.qual<i32, @standard.integer>

function Binop_Overflow (A, B : Integer) return Integer is
   type Byte is mod 256;
   function Wrap (X, Y : Byte) return Byte is
   begin
      return X + Y;
   end Wrap;
   function Scale (F, G : Float) return Float is
   begin
      return F * G;
   end Scale;
begin
   return A + B;
end Binop_Overflow;
