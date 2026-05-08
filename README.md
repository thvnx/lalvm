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
lalvm [--emit=<action>] [--x=<input-type>] <input-file>
```

Options:
- `--emit {ast,mlir,llvm}` — output format (default: llvm)
- `--x {Ada,mlir}` — input type (default: Ada)

## Example

Given `test.adb`:

```ada
function Test (I, J, K : Integer) return Integer is
begin
   return I + J + K;
end Test;
```

```sh
lalvm --emit=ast  test.adb   # Libadalang AST
lalvm --emit=mlir test.adb   # Ada MLIR dialect
lalvm --emit=llvm test.adb   # LLVM IR
```

## Architecture

The compiler is organized in three layers:

- **Ada dialect** (`include/ada/`, `mlir/Dialect.cpp`) — custom MLIR dialect with Ada-level operations
- **MLIRGen** (`mlir/MLIRGen.cpp`) — lowers Libadalang AST to Ada dialect IR
- **LowerToLLVM** (`mlir/LowerToLLVM.cpp`) — lowers Ada dialect to LLVM dialect via Func and Arith dialects

Ada parsing is handled by [Libadalang](https://github.com/AdaCore/libadalang) through its C API (`include/lal/AST.h`, `parser/AST.cpp`).

## Testing

Tests use [lit](https://llvm.org/docs/CommandGuide/lit.html) and FileCheck. Each test
file in `test/` carries its own `RUN` and `CHECK` directives.

```sh
pip install lit   # one-time
cmake --build --preset=debug --target check-lalvm
```

## Dependencies

- LLVM/MLIR 21
- Libadalang
- lit (for running tests)
