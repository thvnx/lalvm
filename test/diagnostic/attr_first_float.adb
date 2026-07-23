-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- 'First on a non-integer subtype: float bounds wait on `float_info` range
-- metadata, so the prefix is diagnosed.

-- CHECK: error: 'first' is only supported on integer subtypes

function Attr_First_Float return Float is
begin
   return Float'First;
end Attr_First_Float;
