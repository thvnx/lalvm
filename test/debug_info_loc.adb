-- RUN: %lalvm --emit=mlir --mlir-print-debuginfo --mlir-print-local-scope %s | %FileCheck %s

-- CHECK-LABEL: ada.subp @debug_info_loc(
-- CHECK-SAME:    %arg0: !ada.qual<i32, @standard.integer> loc("i"("{{.*}}debug_info_loc.adb":13:26 to :27))
-- CHECK-SAME:    %arg1: !ada.qual<i32, @standard.integer> loc("j"("{{.*}}debug_info_loc.adb":13:29 to :30))
-- CHECK-SAME:    %arg2: !ada.qual<i32, @standard.integer> loc("k"("{{.*}}debug_info_loc.adb":13:32 to :33))
-- CHECK:         %{{.*}} = ada.binop "+" %arg0, %arg1 : !ada.qual<i32, @standard.integer> loc("{{.*}}debug_info_loc.adb":15:13 to :14)
-- CHECK:         %{{.*}} = ada.binop "+" %{{.*}}, %arg2 : !ada.qual<i32, @standard.integer> loc("{{.*}}debug_info_loc.adb":15:17 to :18)
-- CHECK:         ada.return %{{.*}} : !ada.qual<i32, @standard.integer> loc("{{.*}}debug_info_loc.adb":15:4 to :21)
-- CHECK:       } loc("{{.*}}debug_info_loc.adb":13:1 to 16:20)
-- CHECK:     } loc("{{.*}}debug_info_loc.adb":13:1 to 16:20)

function Debug_Info_Loc (I, J, K : Integer) return Integer is
begin
   return I + J + K;
end Debug_Info_Loc;
