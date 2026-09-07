# LALVM: Ada to LLVM Compiler

LALVM is a prototype Ada-to-LLVM compiler using AdaCore's
[Libadalang](https://github.com/AdaCore/libadalang) as the Ada frontend,
[MLIR](https://mlir.llvm.org/) as an intermediate representation, and
[LLVM](https://llvm.org/) for the backend.

This README covers building and day-to-day use. See the [Documentation
section](#documentation) below for more content about the architecture and
pipeline, symbol naming and debug info, the supported Ada subset, and the
development workflow (testing/formatting/code
coverage/sanitizers/documentation).

## Disclaimer

This project is an experiment to demonstrate that Libadalang can be used as a
frontend and directly feed LLVM through MLIR. It is in no way a substitute for
any available Ada compiler, and is far from usable today. Many limitations
remain to be addressed, both in Libadalang and in MLIR, before the Ada language
could be fully supported.

## Building

### Dependencies

LALVM is currently based on LLVM/MLIR version 22.

- LLVM/MLIR (for example on Debian: `llvm-dev`, `llvm-22-tools`,
  `libmlir-22-dev`, `mlir-22-tools`, `cmake`, `ninja-build`, `clang`,
  `clang-format`).
- AdaCore's [Libadalang](https://github.com/AdaCore/libadalang) for Ada semantic
  analysis and name resolution.
- `lit` and `FileCheck` for running tests (from `llvm-22-tools`).
- `doxygen` and `graphviz` (optional, for API documentation with call graphs).

### Configuration

Copy the example user presets file and fill in the paths for your environment:

```sh
cp CMakeUserPresets.json.example CMakeUserPresets.json
# edit CMakeUserPresets.json: set LIBADALANG_DIR, MLIR_DIR, compilers
```

`LIBADALANG_DIR` points at a Libadalang build tree (the `build/` directory of a
source checkout, or the crate directory `alr get --build libadalang` produces)
or at an install prefix. LALVM links the shared library, so build Libadalang
with `LIBRARY_TYPE=relocatable`.

Then configure and build:

```sh
cmake --preset=debug
cmake --build --preset=debug
```

`CMakeUserPresets.json` is gitignored; each developer maintains their own copy.
`CMakePresets.json` is committed and holds the shared build settings (Ninja,
Debug mode, assertions enabled).

## Usage

```
lalvm [--emit=<action>] [-P <project.gpr>] [-o <file>] <input-file>
```

Options:
- `--emit {ast,mlir,llvm,obj,asm}`: output format (default: `obj`).
- `-x {Ada,mlir}`: input type (default: Ada; or inferred from the input file
  extension).
- `-o <file>`: output file (default: stdout for text dumps; the input basename
  with a `.o`/`.s` extension for `obj`/`asm`).
- `-P <project.gpr>` (alias `--project`): load a GPR project so `with`ed units
  resolve through its unit provider (otherwise the single file is parsed alone).
- `-O {0,1}`: optimization level (default `0`; `1` runs `mem2reg`).
- `-g`: generate DWARF debug info (off by default).
- `--record-command-line`: record the invocation in `llvm.commandline` (off by
  default; it embeds input/output paths).
- Standard LLVM codegen flags (`-mcpu`, `-mattr`, `--relocation-model`, ...)
  apply to `--emit=obj`/`asm`.

### Example

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
lalvm --emit=ast  compute.adb      # Libadalang AST dump
lalvm --emit=mlir compute.adb      # Ada MLIR dialect (-O0: locals in memory)
lalvm --emit=mlir -O1 compute.adb  # Ada MLIR dialect (mem2reg promotes locals)
lalvm --emit=llvm compute.adb      # LLVM IR
```

MLIR output:
```mlir
module @compute {
  ada.type @standard.integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.subp @compute(%arg0: !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer> {
    %0 = ada.call @compute.double(%arg0) : (!ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
    ada.decls {
      ada.subp private @compute.double(%arg1: !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer> {
        %1 = ada.binop "+" %arg1, %arg1 checks<overflow> : !ada.qual<i32, @standard.integer>
        ada.return %1 : !ada.qual<i32, @standard.integer>
      }
    }
    ada.return %0 : !ada.qual<i32, @standard.integer>
  }
}
```

Nested subprograms are kept in an `ada.decls` symbol container under their
enclosing subprogram and carry a qualified `private` name. The closure
conversion and hoisting passes flatten them to module level on the `--emit=llvm`
path. The output above is from `-O1`, where `mem2reg` has promoted the `Result`
local so the call result flows straight into the `return`. At the default `-O0`
it stays in memory as an `ada.alloca` with a store and load.

### Generating machine code

LALVM can run the LLVM backend itself to emit target assembly or an object file
for the host target:

```sh
lalvm --emit=asm compute.adb         # -> compute.s
lalvm --emit=obj compute.adb         # -> compute.o
lalvm --emit=asm compute.adb -o -    # assembly to stdout
```

Without `-o`, `obj`/`asm` output goes to the input basename with a `.o`/`.s`
extension. CPU and codegen tuning use the standard LLVM flags (`-mcpu`,
`-mattr`, `--relocation-model`, ...).

To target a non-host architecture, pipe the LLVM IR to `llc` (for now, LALVM
itself only emits for the host):

```sh
lalvm --emit=llvm add.adb | llc -mtriple=aarch64-linux-gnu -o add.s
```

## Documentation

- [Architecture and pipeline](docs/architecture.md): the `--emit` pipeline and
  the dialect/pass breakdown.
- [Symbol naming and debug info](docs/abi.md): the GNAT ABI and DWARF output.
- [Feature status](docs/status.md): the supported Ada subset.
- [Development](docs/development.md): testing, coverage, sanitizers, formatting,
  and API docs.
