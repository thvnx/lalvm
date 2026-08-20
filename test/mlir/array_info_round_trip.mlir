// Round-trip ArrayTypeInfoAttr in isolation, carried as a discardable attribute
// on nested modules so it needs neither the !ada.array type nor TypeOp::verify.
// Each CHECK mirrors its input, asserting parse-then-print is identity.

// RUN: %lalvm --emit=mlir %s | %FileCheck %s

module {
  // 1-D static: array (1 .. 10) of Integer.
  // CHECK: module attributes {test.info = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to 10>}
  module attributes {test.info = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to 10>} {}

  // Multidim static: component once, one dim/range clause per dimension.
  // CHECK: module attributes {test.info = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to 10, dim @standard.integer range 1 to 3>}
  module attributes {test.info = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to 10, dim @standard.integer range 1 to 3>} {}

  // Half-static: static lower, dynamic upper, so the attr keeps range 1 to ?.
  // CHECK: module attributes {test.info = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to ?>}
  module attributes {test.info = #ada.array_info<component @standard.integer, dim @standard.integer range 1 to ?>} {}

  // Fully dynamic: both bounds ?.
  // CHECK: module attributes {test.info = #ada.array_info<component @standard.integer, dim @standard.integer range ? to ?>}
  module attributes {test.info = #ada.array_info<component @standard.integer, dim @standard.integer range ? to ?>} {}

  // Unconstrained: array (Integer range <>) of Integer, the dim has no range.
  // CHECK: module attributes {test.info = #ada.array_info<component @standard.integer, dim @standard.integer>}
  module attributes {test.info = #ada.array_info<component @standard.integer, dim @standard.integer>} {}
}
