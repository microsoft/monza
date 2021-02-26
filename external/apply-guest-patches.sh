#!/bin/sh

cd llvm-project
git apply ../patches/llvm-clang.patch
git apply ../patches/llvm-libcxx.patch
cd ..
cd musl
git apply ../patches/musl.patch
cd ..
cd openssl
git apply ../patches/openssl.patch
