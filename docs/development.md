# Development

## Testing

Tests use [lit](https://llvm.org/docs/CommandGuide/lit.html) and FileCheck from
the LLVM installation (`llvm-22-tools` on Debian). `LLVM_EXTERNAL_LIT` can be
used to select another `lit`. Each test file in `test/` carries its own `RUN`
and `CHECK` directives.

```sh
cmake --build --preset=debug --target check-lalvm
```

## Code coverage

Coverage of LALVM's own sources from the test suite uses clang source-based
coverage (`llvm-profdata`/`llvm-cov`). The `coverage` preset (included in the
example user presets) builds an instrumented lalvm into a separate `build-cov/`
tree, so the regular `build/` stays uninstrumented:

```sh
cmake --preset=coverage                                    # configure once
cmake --build --preset=coverage --target check-lalvm       # run suite, fill build-cov/profiles/
cmake --build --preset=coverage --target coverage-report   # merge and render
```

`coverage-report` prints per-file summaries (regions, functions, lines,
branches) and writes line-level HTML reports, keeping the hand-written sources
and the tablegen-generated files separate: `build-cov/coverage/html/` for the
sources, `html-generated/` for the generated files. The prebuilt MLIR/LLVM
libraries are not instrumented. Each `check-lalvm` run starts from a clean
profile pool, so the report always reflects the last run.

## Sanitizers

The `sanitizers` preset (`LLVM_USE_SANITIZER=Address;Undefined`) builds LALVM
with [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html),
[LeakSanitizer](https://clang.llvm.org/docs/LeakSanitizer.html) and
[UndefinedBehaviorSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
into `build-san/`. Only LALVM's own sources are instrumented, not the prebuilt
MLIR/LLVM and Libadalang libraries. It requires the compiler-rt runtime
(`libclang-rt-<N>-dev` on Debian).

```sh
cmake --preset=sanitizers
cmake --build --preset=sanitizers --target check-lalvm
```

A sanitizer error fails the test that triggers it, with the report in the test
output. Reports coming only from the GNAT runtime or Libadalang can be silenced
with a suppressions file, given through `ASAN_OPTIONS`, `LSAN_OPTIONS`, or
`UBSAN_OPTIONS`, which lit forwards to the tests:

```sh
LSAN_OPTIONS=suppressions=/path/to/lsan.supp cmake --build --preset=sanitizers --target check-lalvm
```

Under ASan, lit sets `ASAN_OPTIONS=allow_user_poisoning=0` to avoid a spurious
use-after-poison from the uninstrumented libraries. Set it too for manual runs.

## API documentation

API documentation is generated with Doxygen (requires `doxygen` and `graphviz`):

```sh
cmake --build --preset=debug --target doxygen
```

The output is written to `build/docs/html/`.

## Formatting

Source files are formatted with `clang-format` using the LLVM style. To reformat
all C++ sources in place:

```sh
cmake --build --preset=debug --target format
```
