-- RUN: %lalvm -P %S/pkg.gpr %s --emit=mlir | %FileCheck %s

-- -P loads the GPR project so libadalang resolves `with Config;`: Small is a
-- subtype declared in config.ads (in the project's source dir), made directly
-- visible by `use Config;`. Without -P the `with` would not resolve. The
-- subtype's range comes from the withed unit.

-- CHECK-DAG: ada.subp @main
-- CHECK-DAG: ada.type @config.small base @standard.integer : i32 = #ada.int_info<range 1 to 10>

with Config;
use Config;
function Main return Integer is
   X : Small := 5;
begin
   return X;
end Main;
