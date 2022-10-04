# Creating Monza apps

Any application using a CMake build system can be compiled to be compatible with and linked against Monza, but the following options need to be set for the CMake project:
* The `MONZA_LLVM_LOCATION` needs to be set to point to the Monza-enabled compiler binaries.
* The build tools need to be set as follows before the top-level CMake project is defined:
```
set(CMAKE_C_COMPILER ${MONZA_LLVM_LOCATION}/bin/clang)
set(CMAKE_CXX_COMPILER ${MONZA_LLVM_LOCATION}/bin/clang++)
set(CMAKE_LINKER ${MONZA_LLVM_LOCATION}/bin/ld.lld)
set(CMAKE_AR ${MONZA_LLVM_LOCATION}/bin/llvm-ar)
set(CMAKE_OBJCOPY ${MONZA_LLVM_LOCATION}/bin/llvm-objcopy)
set(CMAKE_NM ${MONZA_LLVM_LOCATION}/bin/llvm-nm)
set(CMAKE_RANLIB ${MONZA_LLVM_LOCATION}/bin/llvm-ranlib)
```
* Modern C and C++ standards need to be enabled for the project:
```
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
```
* It is recommended that you make your CMake project take a dependency on an external project targeting the external folder.
``ExternalProject_Add(external ...)`
* * The current recommended way to do this is to make your project be an `ExternalProject` of another wrapping CMake project.

## Using the Monza application framework

Monza includes an application framework to make it easy to create host-guest pairs that communication via ringbuffers, without the user needing to worry about the details.
The ringbuffer communication uses the implementation from [CCF](https://github.com/microsoft/CCF), which is designed to be fast, secure (specifically hardened to work in a mutual distrust environment).

In addition, the `apps-framework` folder includes code to make it easy to create a new guest instance in a host and automatically connect up to it as well to give a new application endpoint where the ringbuffers are already set up.

For an example of using this framework, refer to the example app in `apps\example`.

## Building a new standalone app targeting Monza

If you don't want to use the application framework, then you can make a particular executable into a Monza image by using
```
target_link_libraries({YourTarget} monza_app)
```
where `{YourTarget}` is an executable target.

The resulting executable can be converted to be bootable into QEMU using
```
add_qemu_compatible_image({NewTarget} {YourTarget})
```

This command creates an image named `{NewTarget}.img` in the current build directory.
