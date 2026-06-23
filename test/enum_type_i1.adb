-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR

-- 2 literals -> i1; On has position 1.
-- MLIR-LABEL: ada.subp @enum_type_i1
-- MLIR:         %[[C:.*]] = ada.constant : !ada.qual<i1, @enum_type_i1.switch> = true
-- MLIR-NEXT:    ada.decls {
-- MLIR-NEXT:      ada.type @enum_type_i1.switch : i1 = #ada.enum_info<"off" = 0, "on" = 1>
-- MLIR:         ada.null
-- MLIR-NEXT:    ada.return

procedure Enum_Type_I1 is
   type Switch is (Off, On);
   D : constant Switch := On;
begin
   null;
end Enum_Type_I1;
