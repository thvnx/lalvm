-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- An out formal denotes an independent object with no initial input.

-- MLIR-LABEL: ada.subp @param_out
-- MLIR:         %[[N_PTR:.*]] = ada.object : !ada.ref<!ada.qual<i32, @standard.integer>>
-- MLIR-NOT:     ada.write {{.*}}, %[[N_PTR]]
-- MLIR:         ada.decls {
-- MLIR:           ada.subp private @param_out.get_value(%x: !ada.ref<!ada.qual<i32, @standard.integer>> {ada.mode = #ada<mode out>})
-- MLIR-NOT:       ada.read %x
-- MLIR:           ada.write {{.*}}, %x : !ada.qual<i32, @standard.integer> -> !ada.ref<!ada.qual<i32, @standard.integer>>
-- MLIR:           ada.return
-- MLIR:         ada.call @param_out.get_value(%[[N_PTR]]) : (!ada.ref<!ada.qual<i32, @standard.integer>>) -> ()
-- MLIR:         %[[N:.*]] = ada.read %[[N_PTR]] : !ada.ref<!ada.qual<i32, @standard.integer>> -> !ada.qual<i32, @standard.integer>
-- MLIR:         ada.return %[[N]] : !ada.qual<i32, @standard.integer>

-- LLVM-LABEL: define i32 @_ada_param_out(
-- LLVM-LABEL: define internal i32 @param_out__get_value()
-- LLVM:          store i32 42, ptr
-- LLVM:          ret i32

function Param_Out return Integer is
   procedure Get_Value (X : out Integer);

   procedure Get_Value (X : out Integer) is
   begin
      X := 42;
   end Get_Value;
   N : Integer;
begin
   Get_Value (N);
   return N;
end Param_Out;
