-- XFAIL: *
-- RUN: %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- Known limitation: named numbers whose static expression resolves to a type
-- other than universal_integer or universal_real are not supported. LAL
-- returns root_real for arithmetic expressions like "1.0 * 3.14159".

-- CHECK-NOT: error: named number has unsupported type 'root_real'

procedure Number_Decl_Expr is
   Pi : constant := 1.0 * 3.14159;
begin
   null;
end Number_Decl_Expr;
