// A mixed array_info (one dimension constrained, one not) is invalid Ada, so
// ArrayTypeInfoAttr's verify rejects it.

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: error: {{.*}}all constrained or all unconstrained

module attributes {test.info = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to 10, dim @standard.integer>} {}
