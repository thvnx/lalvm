#!/usr/bin/env sh

set -x

mkdir -p build
pushd build

cmake -G Ninja .. \
    -DMLIR_DIR=/usr/lib/llvm-20/lib/cmake/mlir \
    -DCMAKE_C_COMPILER=clang-20 \
    -DCMAKE_CXX_COMPILER=clang++-20 \
    -DCMAKE_CXX_FLAGS="-gdwarf-4" \
    -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=mold" \
    -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=mold" \
    -DCMAKE_BUILD_TYPE=Debug

cmake --build .
popd

./build/bin/lalvm --emit=ast test.adb
./build/bin/lalvm --emit=mlir test.adb -mlir-pretty-debuginfo
