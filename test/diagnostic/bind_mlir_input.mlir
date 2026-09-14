// RUN: %not %lalvm --bind %s -o %t.o 2>&1 | %FileCheck %s

// CHECK: Can't bind when the input is MLIR

module {
}
