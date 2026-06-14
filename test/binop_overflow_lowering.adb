-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --implicit-check-not="with.overflow.i8"

-- Signed-integer +/-/* with the overflow flag lower to the LLVM checked
-- intrinsic and trap to `__gnat_rcheck_CE_Overflow_Check` on overflow (RM 4.5).
-- Modular `+` wraps (plain `add`) and float `*` is unchecked (`fmul`); neither
-- emits a checked intrinsic (`--implicit-check-not` guards the modular i8 case).

-- CHECK-LABEL: define i32 @_ada_binop_overflow_lowering(
-- CHECK:         %[[WO:.*]] = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %0, i32 %1)
-- CHECK:         %{{.*}} = extractvalue { i32, i1 } %[[WO]], 0
-- CHECK:         %[[O:.*]] = extractvalue { i32, i1 } %[[WO]], 1
-- CHECK:         br i1 %[[O]], label %[[RAISE:[0-9]+]], label %{{[0-9]+}}
-- CHECK:       [[RAISE]]:
-- CHECK:         call void @__gnat_rcheck_CE_Overflow_Check(ptr {{.*}}, i32 {{.*}})
-- CHECK:         unreachable

-- The other signed operators emit their matching checked intrinsics; modular
-- `+` lowers to a plain wrapping `add i8` and float `*` to `fmul`.
-- CHECK-DAG:     call { i32, i1 } @llvm.ssub.with.overflow.i32
-- CHECK-DAG:     call { i32, i1 } @llvm.smul.with.overflow.i32
-- CHECK-DAG:     = add i8
-- CHECK-DAG:     = fmul float

function Binop_Overflow_Lowering (A, B : Integer) return Integer is
   type Byte is mod 256;
   function Diff (X, Y : Integer) return Integer is
   begin
      return X - Y;
   end Diff;
   function Prod (X, Y : Integer) return Integer is
   begin
      return X * Y;
   end Prod;
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
end Binop_Overflow_Lowering;
