# Development

## Testing

Tests use [lit](https://llvm.org/docs/CommandGuide/lit.html) and FileCheck. Each
test file in `test/` carries its own `RUN` and `CHECK` directives.

```sh
pip install lit   # one-time
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
