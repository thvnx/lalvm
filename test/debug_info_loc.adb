-- RUN: %lalvm --emit=mlir --mlir-print-debuginfo --mlir-print-local-scope %s | %FileCheck %s

-- CHECK-LABEL: ada.func @test(
-- CHECK-SAME:    %arg0: i32 loc("I"("{{.*}}debug_info_loc.adb":13:16 to :17))
-- CHECK-SAME:    %arg1: i32 loc("J"("{{.*}}debug_info_loc.adb":13:19 to :20))
-- CHECK-SAME:    %arg2: i32 loc("K"("{{.*}}debug_info_loc.adb":13:22 to :23))
-- CHECK:         %{{.*}} = ada.binop "+" %arg0, %arg1 : i32 loc("{{.*}}debug_info_loc.adb":15:13 to :14)
-- CHECK:         %{{.*}} = ada.binop "+" %{{.*}}, %arg2 : i32 loc("{{.*}}debug_info_loc.adb":15:17 to :18)
-- CHECK:         ada.return %{{.*}} : i32 loc("{{.*}}debug_info_loc.adb":15:4 to :21)
-- CHECK:       } loc("{{.*}}debug_info_loc.adb":13:1 to 16:10)
-- CHECK:     } loc("{{.*}}debug_info_loc.adb":13:1 to 16:10)

function Test (I, J, K : Integer) return Integer is
begin
   return I + J + K;
end Test;
