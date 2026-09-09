# Symbol naming and debug info

## Symbol naming (ABI)

LALVM follows GNAT's symbol naming convention:

- **Library-level subprograms** get the `_ada_` prefix: `procedure Foo` becomes
  `_ada_foo`.
- **Nested subprograms** get a `parent__child` mangled name without the prefix:
  `procedure Inner` inside `procedure Outer` becomes `outer__inner`.
- **Operator subprograms** are mapped to GNAT O-names before mangling: `function
  "*"` nested inside `procedure Outer` becomes `outer__Omultiply`.

Using the same name mangling convention allows `lalvm`-compiled code to be
linked against `gnat`-compiled code, which is valuable while LALVM is still
incomplete and leans on the GNAT toolchain and runtime.

## Debug info

Debug info is emitted only under `-g`. Without it no `.debug_*` sections are
produced (source locations still ride on every op for diagnostics and exception
messages). With `-g`, LALVM emits DWARF 5 with `DW_LANG_Ada2012`, carried from
the MLIR op locations through to LLVM IR.

Each named Ada object (variable, constant, named number, parameter, ...) carries
a `NameLoc` in the Ada dialect, which `AdaDebugInfoPass` consumes to emit
`dbg.declare` (for stack variables) or `dbg.value` (for constants and named
numbers). Enum types produce `DICompositeType` entries with one `DIEnumerator`
per literal. Constrained integer subtypes produce `DISubrangeType` entries with
their bounds: constants for static bounds, and a referenced `DILocalVariable`
for a dynamic bound (see the limitations in `AdaDebugInfoPass`). Statically
constrained arrays produce a `DW_TAG_array_type` named after the Ada type, with
one `DW_TAG_subrange_type` per dimension typed by the index type and carrying
both bounds, as GNAT emits. Ada `goto` labels get a `DW_TAG_label` (via
`llvm.intr.dbg.label`), so a debugger can break on a labeled statement.

To inspect source locations in the MLIR output, use:

```sh
lalvm --emit=mlir --mlir-print-debuginfo --mlir-print-local-scope add.adb
```

This prints each op's source location inline, for example:

```mlir
%0 = ada.binop "+" %arg0, %arg1 : !ada.qual<i32, @standard.integer> loc("add.adb":15:13 to :14)
```
