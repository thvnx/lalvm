# LALVM — Ada to LLVM Compiler

LALVM is a prototype Ada-to-LLVM compiler using MLIR as an intermediate representation.

## Pipeline

```
Ada source → Libadalang AST → Ada MLIR dialect → Intermediate dialects → LLVM dialect → LLVM IR
```

## Building

```sh
./build.sh
```

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
cmake --build build --target check-lalvm
```

## Dependencies

- LLVM/MLIR 21
- Libadalang
- lit (for running tests)
