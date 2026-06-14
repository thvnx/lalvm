-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Recursive factorial via a nested function and an if statement (RM 5.3): the
-- base case returns 1, the recursive case returns F (X - 1) * X.

-- MLIR-LABEL: ada.subp @facto
-- MLIR:         ada.subp private @facto.f(%arg0: !ada.qual<i32, @standard.integer>)
-- MLIR:           ada.cmp "=" %arg0, %{{.*}} : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i1, @standard.boolean>
-- MLIR:           ada.unwrap %{{.*}} : !ada.qual<i1, @standard.boolean> to i1
-- MLIR:           cf.cond_br
-- MLIR:           ada.return %{{.*}} : !ada.qual<i32, @standard.integer>
-- MLIR:           ada.call @facto.f
-- MLIR:           ada.binop "*"
-- MLIR:           ada.return
-- MLIR:         ada.call @facto.f

-- LLVM:      define internal i32 @facto__f(
-- LLVM:        icmp eq i32 %0, 0
-- LLVM:        br i1
-- LLVM:        ret i32 1
-- LLVM:        call { i32, i1 } @llvm.ssub.with.overflow.i32(i32 %0, i32 1)
-- LLVM:        call i32 @facto__f
-- LLVM:        call { i32, i1 } @llvm.smul.with.overflow.i32
-- LLVM:        ret i32

function Facto return Integer is
   function F (X : Integer) return Integer is
   begin
      if X = 0 then
         return 1;
      else
         return F (X - 1) * X;
      end if;
   end F;
begin
   return F (5);
end Facto;
