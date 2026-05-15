# LALVM — Ada to LLVM Compiler

LALVM is a prototype Ada-to-LLVM compiler using MLIR as an intermediate representation.

## Pipeline

```
Ada source → Libadalang AST → Ada MLIR dialect → Intermediate dialects → LLVM dialect → LLVM IR
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

`CMakeUserPresets.json` is gitignored — each developer maintains their own copy.
`CMakePresets.json` is committed and holds the shared build settings (Ninja, Debug
mode, assertions enabled).

## Usage

```
lalvm --emit=<action> [--x=<input-type>] <input-file>
```

Options:
- `--emit {ast,mlir,llvm}` — output format (required)
- `--x {Ada,mlir}` — input type (default: Ada; inferred from `.mlir` extension)

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
ada.func @compute(%arg0: i32) -> i32 {
  ada.func @double(%arg0: i32) -> i32 {
    %0 = ada.binop "+" %arg0, %arg0 : i32
    ada.return %0 : i32
  }
  %0 = ada.call @double(%arg0) : (i32) -> i32
  ada.return %0 : i32
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
  `procedure Foo` → `_ada_foo`
- **Nested subprograms** get a `parent__child` mangled name without the prefix:
  `procedure Inner` inside `procedure Outer` → `outer__inner`

This allows lalvm-compiled code to be linked against GNAT-compiled code and
called from C using the same name mangling convention.

## Debug info

LALVM emits DWARF 5 debug info with `DW_LANG_Ada2012`. Source locations are
attached to every MLIR operation and carried through to LLVM IR.

To inspect source locations in the MLIR output:

```sh
lalvm --emit=mlir --mlir-print-debuginfo --mlir-print-local-scope add.adb
```

This prints each op's source location inline, for example:

```mlir
%0 = ada.binop "+" %arg0, %arg1 : i32 loc("add.adb":15:13 to :14)
```

## Status

Work in progress. Only a small subset of Ada is supported; most language features
are not yet implemented.

- **Expressions (partial):** integer and real literals, binary arithmetic (`+`, `-`, `*`),
  variable references, function calls
- **Statements (partial):** assignments, `return`, `null`, procedure calls, block
  statements (`begin`/`end` and `declare`/`begin`/`end`)
- **Declarations (partial):** function and procedure subprograms (library-level and
  nested), local variable declarations with initializers, named numbers
- **Types:** `Integer` (i32), `Short_Integer` (i16), `Long_Integer` (i64),
  `Float` (f32), `Long_Float` (f64)
- **Debug info:** DWARF 5, `DW_LANG_Ada2012`, source locations on all ops

## Architecture

The compiler is organized in three layers:

- **Ada dialect** (`include/ada/`, `mlir/Dialect.cpp`) — custom MLIR dialect defining
  `ada.func`, `ada.proc`, `ada.return`, `ada.binop`,
  `ada.call`, `ada.block_stmt`, `ada.null`
- **MLIRGen** (`mlir/MLIRGen.cpp`) — lowers a Libadalang AST to the Ada dialect
- **LowerToLLVM** (`mlir/LowerToLLVM.cpp`) — lowers the Ada dialect to LLVM IR via
  the Func and Arith intermediate dialects

Ada parsing is handled by [Libadalang](https://github.com/AdaCore/libadalang) through
its C API (`include/lal/AST.h`, `parser/AST.cpp`).

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
