// A malformed `.mlir` input (here `%missing`, an undefined SSA value) gets a
// located diagnostic echoing the source line, via loadMLIRFile's SourceMgr
// handler (the echoed line is what the default handler can't print).

// RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

// CHECK: {{.*}}mlir_parse_error.mlir:{{[0-9]+}}:{{[0-9]+}}: error:
// CHECK: ada.return %missing

module {
  ada.subp @f() -> !ada.qual<i32, @standard.integer> {
    ada.return %missing : !ada.qual<i32, @standard.integer>
  }
}
