# Guest

The guest represents the collection of components that the application compiles and links against to form the VM image.
It represents the actual unikernel.
The code for it is located in the `guest-verona-rt` folder.
The naming of the folder is such as to highlight that it is closely tied to the [Project Verona](https://github.com/microsoft/verona) language runtime.

## Components
* [C standard library](./libc.md).
* [C++ standard library](./libc++.md).
* [Concurrency based on Project Verona](./concurrency.md).
* [Cryptography and TLS](./crypto.md).
* [Bootloader for QEMU](./bootloader.md).
* Monza internals for booting and system managment.
* Test apps.
* [Experimental features](./experimental.md).

## Porting applications to run with the Monza guest
Monza is specifically not targeting POSIX compatibility, as such porting applications should be done with care.
The first thing to consider before attempting any porting, is to identify how the concurrency model of Monza would be usable within the application to be ported.
Once the story regarding concurrency is clear, the C/C++ standard libraries should be analyzed for compatibility.
