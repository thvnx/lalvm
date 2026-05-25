# LALVM: Ada to LLVM Compiler

LALVM is a prototype Ada-to-LLVM compiler using MLIR as an intermediate representation.

## Pipeline

```
Ada source -> Libadalang AST -> Ada MLIR dialect -> Intermediate dialects -> LLVM dialect -> LLVM IR
```

## Building

Copy the example user presets file and fill in the paths for your environment:

```sh
cp CMakeUserPresets.json.example CMakeUserPresets.json
# edit CMakeUserPresets.json: set LIBADALANG_INCLUDE_DIR, MLIR_DIR, compilers, lit
```

Then configure and build:

```sh
cmake --preset=debug
cmake --build --preset=debug
```

`CMakeUserPresets.json` is gitignored; each developer maintains their own copy.
`CMakePresets.json` is committed and holds the shared build settings (Ninja, Debug
mode, assertions enabled).

## Usage

```
lalvm --emit=<action> [--x=<input-type>] <input-file>
```

Options:
- `--emit {ast,mlir,llvm}`: output format (required)
- `--x {Ada,mlir}`: input type (default: Ada; inferred from `.mlir` extension)

## Example

```ada
-- compute.adb
function Compute (X : Integer) return Integer is
   function Double (N : Integer) return Integer is
   begin
      return N + N;
   end Double;
   Result : Integer := Double (X);
begin
   return Result;
end Compute;
```

```sh
lalvm --emit=ast  compute.adb   # Libadalang AST dump
lalvm --emit=mlir compute.adb   # Ada MLIR dialect
lalvm --emit=llvm compute.adb   # LLVM IR
```

MLIR output:
```mlir
ada.type @standard.integer : i32 = #ada.numeric_info
ada.subp @compute(%arg0: i32 {ada.type = @standard.integer}) -> i32 {
  ada.subp @double(%arg0: i32 {ada.type = @standard.integer}) -> i32 {
    %0 = ada.binop "+" %arg0, %arg0 {ada.type = @standard.integer} : i32
    ada.return %0 : i32
  }
  %c0_i32 = arith.constant {ada.type = @standard.integer} 0 : i32
  %alloca = memref.alloca() {ada.type = @standard.integer} : memref<i32>
  %1 = ada.call @double(%arg0) {ada.type = @standard.integer} : (i32) -> i32
  memref.store %1, %alloca[] {ada.type = @standard.integer} : memref<i32>
  %2 = memref.load %alloca[] : memref<i32>
  ada.return %2 : i32
}
```

## Generating assembly

The LLVM IR produced by `--emit=llvm` can be passed to `llc` to generate assembly
for any target supported by LLVM:

```sh
# Native target
lalvm --emit=llvm add.adb | llc -o add.s

# Specific target (e.g. AArch64)
lalvm --emit=llvm add.adb | llc -mtriple=aarch64-linux-gnu -o add.s

# Object file
lalvm --emit=llvm add.adb | llc -filetype=obj -o add.o
```

## Symbol naming (ABI)

LALVM follows GNAT's symbol naming convention:

- **Library-level subprograms** get the `_ada_` prefix:
  `procedure Foo` -> `_ada_foo`
- **Nested subprograms** get a `parent__child` mangled name without the prefix:
  `procedure Inner` inside `procedure Outer` -> `outer__inner`
- **Operator subprograms** are mapped to GNAT O-names before mangling:
  `function "*"` nested inside `procedure Outer` -> `outer__Omultiply`

This allows lalvm-compiled code to be linked against GNAT-compiled code and
called from C using the same name mangling convention.

## Debug info

LALVM emits DWARF 5 debug info with `DW_LANG_Ada2012`. Source locations are
attached to every MLIR operation and carried through to LLVM IR.

Each named Ada object (variable, constant, named number, parameter) carries a
`NameLoc` in the Ada dialect, which `AdaDebugInfoPass` consumes to emit
`dbg.declare` (for stack variables) or `dbg.value` (for constants and named
numbers). Enum types produce `DICompositeType` entries with one `DIEnumerator`
per literal.

To inspect source locations in the MLIR output:

```sh
lalvm --emit=mlir --mlir-print-debuginfo --mlir-print-local-scope add.adb
```

This prints each op's source location inline, for example:

```mlir
%0 = ada.binop "+" %arg0, %arg1 : i32
    loc(fused<@standard.integer>["add.adb":15:13])
```

## Status

Work in progress. Only a subset of Ada is supported; most language features
are not yet implemented.

**Expressions:**
- Integer and real literals; named numbers (RM 3.3.2)
- Binary arithmetic: `+`, `-`, `*`, `/`
- Variable and parameter references
- Function calls (including nested)
- Enum literals

**Statements:**
- Assignments, `return`, `null`
- Procedure calls
- Block statements (`begin`/`end` and `declare`/`begin`/`end`)

**Declarations:**
- Subprograms: functions and procedures, library-level and nested
- Local variables with and without initializers (multiple names per declaration)
- Named numbers (`N : constant := 42`)
- Type declarations: integer, float, modular integer, enum (including `Boolean`)
- User-defined operator functions (definitions and calls; mangled to GNAT O-names)

**Parameters:** `in` (by value), `in out` and `out` (by reference)

**Types:**
- `Integer` (i32), `Short_Integer` (i16), `Long_Integer` (i64)
- `Float` (f32), `Long_Float` (f64)
- `Boolean` (i1) and user-defined enum types (i1 for 2 literals, i8 for 3-256)

**Debug info:** DWARF 5, `DW_LANG_Ada2012`, source locations on all ops,
`dbg.declare`/`dbg.value` for variables and constants, `DICompositeType`
for enum types

## Architecture

The compiler is organized in three layers:

- **Ada dialect** (`include/ada/`, `mlir/Dialect.cpp`): custom MLIR dialect.
  Operations: `ada.type`, `ada.subp`, `ada.return`, `ada.binop`, `ada.call`,
  `ada.block`, `ada.null`. Each op carries Ada-level type metadata via
  an `"ada.type"` attribute (a symbol reference to the relevant `ada.type` op)
  and source name via `NameLoc` where applicable.
- **MLIRGen** (`mlir/MLIRGen.cpp`): lowers a Libadalang AST to the Ada
  dialect. Emits bare Ada names; no ABI mangling.
- **LowerToLLVM** (`mlir/LowerToLLVM.cpp`): lowers the Ada dialect to LLVM
  IR. Owns all ABI concerns: `_ada_` prefix for library-level subprograms,
  `parent__child` mangling for nested ones, GNAT O-name mangling for
  operator subprograms.
- **AdaDebugInfoPass** (`mlir/AdaDebugInfo.cpp`): post-lowering pass that
  emits `dbg.declare`/`dbg.value` intrinsics from `NameLoc` annotations on
  LLVM ops.

Ada parsing is handled by [Libadalang](https://github.com/AdaCore/libadalang)
through its C API (`include/frontend/AST.h`, `frontend/AST.cpp`).

## Testing

Tests use [lit](https://llvm.org/docs/CommandGuide/lit.html) and FileCheck. Each test
file in `test/` carries its own `RUN` and `CHECK` directives.

```sh
pip install lit   # one-time
cmake --build --preset=debug --target check-lalvm
```

## Documentation

API documentation is generated with Doxygen (requires doxygen and graphviz):

```sh
cmake --build --preset=debug --target doxygen
```

The output is written to `build/docs/html/`.

## Formatting

Source files are formatted with clang-format using the LLVM style. To reformat all
C++ sources in place:

```sh
cmake --build --preset=debug --target format
```

## Dependencies

- LLVM/MLIR 21 (for example on Debian: llvm-dev, libmlir-21-dev, mlir-21-tools, cmake, ninja-build, clang, clang-format)
- Libadalang
- lit (for running tests)
- doxygen and graphviz (optional, for API documentation with call graphs)
