-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- An attribute other than 'First/'Last is diagnosed ('Size here; the scalar
-- attributes of RM 3.5.5 are a tracked TODO in `mlirGenAttributeRef`).

-- CHECK: error: unsupported attribute 'size'

function Attr_Unsupported return Integer is
begin
   return Integer'Size;
end Attr_Unsupported;
