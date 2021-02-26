# C++ standard library for Monza

The C++ standard library for Monza is based on [libc++](https://libcxx.llvm.org/) with a custom subsetting focussed around in-memory operations.
It includes a new build option to remove support for `fstream`.
In addition, the existing options to disable `filesystem` and `stdin` support are also enabled.
The external threading support is used to implement support for certain thread-related functionality needed by Project Verona.

Be mindful that while `std::mutex` and `std::condition_variable` are available, using them can result in unexpected interation with the Verona-base concurrency.
Their usage should be avoided, unless the user understands the implications.

In practice, the header-based nature of the C++ standard library makes it difficult to reason about support as unused functions
are never linked to detect missing dependencies from the C standard library.
As such we cannot make easy claims about the available level of support.

## C++ ABI

In order to support C++ language features, Monza uses [libcxxrt](https://github.com/libcxxrt/libcxxrt) as the library underneath the standard library, which implements
features such as exceptions and RTTI.
Monza supports all the functionality available in libcxxrt.