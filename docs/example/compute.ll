; ModuleID = 'compute.adb'
source_filename = "compute.adb"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@lalvm.file = private constant [12 x i8] c"compute.adb\00"

declare void @__gnat_rcheck_CE_Overflow_Check(ptr, i32)

define i32 @_ada_compute(i32 %0) {
  %2 = call i32 @compute__double(i32 %0)
  ret i32 %2
}

define internal i32 @compute__double(i32 %0) {
  %2 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %0, i32 %0)
  %3 = extractvalue { i32, i1 } %2, 0
  %4 = extractvalue { i32, i1 } %2, 1
  br i1 %4, label %5, label %6

5:                                                ; preds = %1
  call void @__gnat_rcheck_CE_Overflow_Check(ptr @lalvm.file, i32 4)
  unreachable

6:                                                ; preds = %1
  ret i32 %3
}

; Function Attrs: nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none)
declare { i32, i1 } @llvm.sadd.with.overflow.i32(i32, i32) #0

attributes #0 = { nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none) }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 2, !"Debug Info Version", i32 3}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{!"lalvm (LLVM 22.1.8, Libadalang undefined)"}
