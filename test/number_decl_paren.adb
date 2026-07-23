-- RUN: %lalvm --emit=mlir %s | %FileCheck %s

-- Named number whose expression is a parenthesized real literal (RM 3.3.2):
-- the parentheses only group syntactically (RM 4.4), so they are stripped at
-- the declaration and the literal folded eagerly, like a bare literal.
-- Nested parentheses strip too.

-- CHECK-LABEL: ada.subp @number_decl_paren()
-- CHECK:         %[[UPI:.*]] = ada.constant : !ada.qual<f128, @standard.universal_real_type_> = {{.*}}
-- CHECK-NEXT:    %[[PI:.*]] = ada.coerce %[[UPI]] : <f128, @standard.universal_real_type_> to <f32, @standard.float>
-- CHECK-NEXT:    ada.return %[[PI]] : !ada.qual<f32, @standard.float>

function Number_Decl_Paren return Float is
   Pi : constant := ((3.14159));
begin
   return Pi;
end Number_Decl_Paren;
