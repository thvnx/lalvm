// `--emit=ast` dumps the Libadalang tree, which does not exist for MLIR input.

// RUN: %not %lalvm --emit=ast %s 2>&1 | %FileCheck %s

// CHECK: Can't dump a Libadalang AST when the input is MLIR

module {
}
