// RUN: %not %lalvm -x Ada --emit=mlir %s 2>&1 | %FileCheck %s

// An explicit `-x Ada` overrides the `.mlir` extension: the file goes to
// Libadalang, which rejects MLIR syntax.

// CHECK: input_type_forced_ada.mlir:{{[0-9]+}}:{{[0-9]+}}: error:

module {
}
