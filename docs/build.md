# Cloning

To clone this repo, you need to pull in the submodules:
```
git clone https://github.com/microsoft/monza
git submodule update --init
```

It is not recommended to force `--recursive` as this pulls in
significant amount of code that is not needed.

## Updating code

To pull the latest code from the master branch, `cd` to the root of the
checkout and run
```
git pull
git submodule update
```

# Building guest and test apps

For reference on what goes into the guest, check the [organization](./organization.md).

## Prerequisites to build on Linux

These steps were tested on Ubuntu 20.04.
The QEMU available in Ubuntu 18.04 does not support RDRAND and is unsuitable.

First, you will need to install dependencies:
```
sudo apt update        # optional, if you haven't updated recently
sudo apt install cmake build-essential ninja-build \
                 clang-12 libc++-12-dev libc++abi-12-dev clang-tools-12 \
                 nasm \
                 clang-format-9 \
                 qemu-system \
                 python3 python3-ecdsa
```

## Apply patches to external repos

```
cd external
./apply-guest-patches.sh
cd ..
```

## Build instructions:

Do this from the repository root and not from the `guest-verona-rt` subdirectory.

You can run
```
mkdir build
cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja
```
to build the release installation with debug info.

Switch the `cmake` line to either
```
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
```
to provide the other configurations.

## Subsequent builds

For subsequent builds, you do not need to rerun `cmake`.
From the `build` directory, you can run
```
ninja
```
