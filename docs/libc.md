# C standard library for Monza

The C standard library for Monza is based on [musl](https://musl.libc.org/) with a custom subsetting focussed around in-memory operations.
There is no support for files or system-level interaction.
Some methods have a custom implementation in `monza-libc`.
The headers are not customized to remove the declarations of unimplemented functions,
but users should expect to receive linker error if attempting to use a functionality unavailable in Monza.
Some functions are available, but with limited functionality:
* Memory mapping:
* * mmap: only MAP_ANONYMOUS | MAP_PRIVATE is supported with no address hint.
* * munmap: partial unmapping is not supported, the range must match a previous mmap allocation fully.
* Localizations:
* * newlocale: only creating a copy of the default C locale is supported (```newlocale(LC_ALL_MASK, "C", nullptr)```).
* * setlocale: only querying the full locale or setting it to the C one is supported (```setlocale(LC_ALL_MASK, "C") || setlocale(LC_ALL_MASK, nullptr)``).
* Timing:
* * time zones: anything time zone related accepts only the "UTC" time zone.
* Pthread:
* * mutex: pthread_mutex is a wrapper around a spin-lock. Highly recommended to avoid using if possible.