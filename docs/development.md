# Development

Follow the [build instructions](../README.md#building) before running these
commands from the repository root. The presets below are provided in
[CMakeUserPresets.json.example](../CMakeUserPresets.json.example). Simply copy
it to `CMakeUserPresets.json` and adjust the paths for your environment.

## Testing

Run the test suite after changes to compiler behavior:

```sh
cmake --build --preset=debug --target check-lalvm
```

Tests use [lit](https://llvm.org/docs/CommandGuide/lit.html) and FileCheck from
the LLVM installation. Each test file in `test/` carries its own `RUN` and
`CHECK` directives. Set `LLVM_EXTERNAL_LIT` when configuring CMake to select
another `lit`.

## Formatting

Format C++ changes with `clang-format` using the LLVM style. To reformat all C++
sources in place:

```sh
cmake --build --preset=debug --target format
```

## API documentation

The [API reference](https://thvnx.github.io/lalvm/) is available online. To
generate it locally with Doxygen, install `doxygen` and `graphviz`, then run:

```sh
cmake --build --preset=debug --target doxygen
```

The output is written to `build/docs/html/`.

## Code coverage

Use coverage to identify compiler paths the test suite does not exercise. The
`coverage` preset uses Clang coverage instrumentation and
`llvm-profdata`/`llvm-cov` in a separate `build-cov/` directory:

```sh
cmake --preset=coverage
cmake --build --preset=coverage --target check-lalvm
cmake --build --preset=coverage --target coverage-report
```

`coverage-report` prints a summary and writes HTML reports to
`build-cov/coverage/html/` for project sources and
`build-cov/coverage/html-generated/` for generated sources. Only LALVM is
instrumented so the prebuilt MLIR/LLVM libraries are excluded. Each test suite
run replaces the previous profile data in `build-cov/profiles/`.

## Sanitizers

Use sanitizers to investigate memory errors and undefined behavior. The
`sanitizers` preset enables
[AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html),
[LeakSanitizer](https://clang.llvm.org/docs/LeakSanitizer.html), and
[UndefinedBehaviorSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
in a separate `build-san/` directory. Install the compiler runtime
(`libclang-rt-<N>-dev`) before configuring:

```sh
cmake --preset=sanitizers
cmake --build --preset=sanitizers --target check-lalvm
```

Only LALVM's sources are instrumented (the prebuilt MLIR/LLVM and Libadalang
libraries are excluded). A sanitizer error fails the test that triggers it, with
the report in the test output.

### Troubleshooting

For manual runs under AddressSanitizer, set
`ASAN_OPTIONS=allow_user_poisoning=0` to avoid false reports caused by
uninstrumented libraries accessing poisoned memory. The test suite sets this
option automatically.

Reports originating solely in the GNAT runtime or Libadalang can be silenced
with a suppressions file. The test suite forwards `ASAN_OPTIONS`,
`LSAN_OPTIONS`, and `UBSAN_OPTIONS`, for example:

```sh
LSAN_OPTIONS=suppressions=/path/to/lsan.supp cmake --build --preset=sanitizers --target check-lalvm
```
