-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- If/elsif/else chain (RM 5.3). Each elsif condition is evaluated in its own
-- block, reached only on the false edge of the previous test, so conditions are
-- tested in order until one is true. All branches return, so there is no merge.

-- MLIR-LABEL: ada.subp @if_elsif
-- MLIR:         ada.cmp "=" %arg0, %{{.*}} : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i1, @standard.boolean>
-- MLIR:         ada.unwrap %{{.*}} : !ada.qual<i1, @standard.boolean> to i1
-- MLIR:         cf.cond_br
-- MLIR:         ada.cmp "=" %arg0, %{{.*}} : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i1, @standard.boolean>
-- MLIR:         cf.cond_br

-- LLVM-LABEL: define i32 @_ada_if_elsif(
-- LLVM-DAG:      icmp eq i32 %0, 0
-- LLVM-DAG:      icmp eq i32 %0, 1
-- LLVM:          ret i32

function If_Elsif (X : Integer) return Integer is
begin
   if X = 0 then
      return 10;
   elsif X = 1 then
      return 20;
   else
      return 30;
   end if;
end If_Elsif;
