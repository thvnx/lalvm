#!/usr/bin/env sh

set -x

# gprbuild -P lalvm.gpr -XLIBRARY_TYPE=relocatable

mkdir -p build
pushd build

cmake -G Ninja .. \
    -DMLIR_DIR=/usr/lib/llvm-20/lib/cmake/mlir \
    -DCMAKE_C_COMPILER=clang-20 \
    -DCMAKE_CXX_COMPILER=clang++-20 \
    -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=mold" \
    -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=mold" \
    -DCMAKE_BUILD_TYPE=Debug

cmake --build .
popd

LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/lib ./build/bin/toy
