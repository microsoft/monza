# Building a new app targeting Monza

Any app using a CMake build system can be compiled to be compatible with and linked against Monza,
by configuring the variable `MONZA_LLVM_LOCATION` to point to the Monza-enabled compiler binaries,
including the `guest-verona-rt` folder as a CMake subdirectory,
and using
```
target_link_libraries({YourTarget} monza_app)
```
where `{YourTarget}` is an executable target.

The resulting executable can be converted to be bootable into QEMU using
```
add_qemu_compatible_image({NewTarget} {YourTarget})
```

This command creates an image named `{NewTarget}.img` in the current build directory.

The new app should also take a dependence on the CMake project in the `external` folder to make sure that the compiler is built.
Use the example in the top-level CMakeLists.txt to create a CMake ExternalProject `ExternalProject_Add(external ...)`.
