-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Iterative factorial via a nested function and a while loop (RM 5.5): the
-- header tests `I <= X` and the body accumulates `Result * I`. Compare with the
-- recursive `facto.adb`.

-- MLIR-LABEL: ada.subp @facto_loop
-- MLIR:         ada.subp private @facto_loop.f(%arg0: !ada.qual<i32, @standard.integer>)
-- MLIR:           cf.br ^[[HDR:bb[0-9]+]](
-- MLIR:         ^[[HDR]](
-- MLIR:           ada.cmp "<=" %{{.*}}, %arg0 : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i1, @standard.boolean>
-- MLIR:           ada.unwrap %{{.*}} : !ada.qual<i1, @standard.boolean> to i1
-- MLIR:           cf.cond_br %{{.*}}, ^[[BODY:bb[0-9]+]], ^[[MERGE:bb[0-9]+]]
-- MLIR:         ^[[BODY]]:
-- MLIR:           ada.binop "*"
-- MLIR:           cf.br ^[[HDR]](
-- MLIR:         ^[[MERGE]]:
-- MLIR:           ada.return
-- MLIR:         ada.call @facto_loop.f

-- LLVM:      define internal i32 @facto_loop__f(
-- LLVM:        phi i32
-- LLVM:        icmp sle i32 %{{.*}}, %0
-- LLVM:        br i1
-- LLVM:        call { i32, i1 } @llvm.smul.with.overflow.i32
-- LLVM:        ret i32

function Facto_Loop return Integer is
   function F (X : Integer) return Integer is
      Result : Integer := 1;
      I      : Integer := 1;
   begin
      while I <= X loop
         Result := Result * I;
         I := I + 1;
      end loop;
      return Result;
   end F;
begin
   return F (5);
end Facto_Loop;
