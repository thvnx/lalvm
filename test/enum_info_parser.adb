-- RUN: %lalvm --emit=mlir %s | %lalvm -x mlir --emit=llvm - | %FileCheck %s

-- Round-trip: MLIR containing #ada.enum_info is parsed back and lowered to
-- LLVM IR. Exercises EnumTypeInfoAttr::parse and TypeOpLowering.

-- CHECK-LABEL: define void @_ada_enum_info_parser()
-- CHECK:         ret void

procedure Enum_Info_Parser is
   type Color is (Red, Green, Blue);
begin
   null;
end Enum_Info_Parser;
