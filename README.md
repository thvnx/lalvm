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
-- add.adb
function Test (I, J, K : Integer) return Integer is
begin
   return I + J + K;
end Test;
```

```sh
lalvm --emit=ast  add.adb   # Libadalang AST dump
lalvm --emit=mlir add.adb   # Ada MLIR dialect
lalvm --emit=llvm add.adb   # LLVM IR
```

MLIR output:
```mlir
ada.func @test(%arg0: i32, %arg1: i32, %arg2: i32) -> i32 {
  %0 = ada.add %arg0, %arg1 : i32
  %1 = ada.add %0, %arg2 : i32
  ada.return %1 : i32
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

## Status

This is a work in progress. Only a very small subset of Ada is currently supported.

## Architecture

The compiler is organized in three layers:

- **Ada dialect** (`include/ada/`, `mlir/Dialect.cpp`) — custom MLIR dialect defining
  `ada.func`, `ada.proc`, `ada.return`, `ada.add`, `ada.sub`, `ada.mul`
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
