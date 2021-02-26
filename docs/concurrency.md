# Concurrency

For concurrency Project Monza uses the [Project Verona](https://github.com/microsoft/verona) language runtime implemented in C++, rather than the pthreads API.
Project Verona offers a brand new concurrency model based on [concurrent ownership](https://github.com/microsoft/verona/blob/master/docs/explore.md).
In this model, there are two key concepts
*  behaviours, and
*  concurrent owners (cowns for short).
Behaviours are the operations of the system, and concurrent owners are the resources.
Each behaviour executes in the context of some (possibly empty) collection of cowns.
The scheduler guarantees forward progress as long as every behaviour can terminate.

## Summary of the API

Including `<cpp/when.h>` gives acces to the following operations and types:
* `verona::cpp::make_cown<T>(args)`: similar to std::make_shared<T>(args), it created a heap object of type T with args as constructor arguments.
  * It then Wraps the object into a `verona::cpp::cown_ptr<T>` smart pointer.
* `verona::cpp::cown_ptr<T>`: is a smart pointer similar to `std::shared_ptr<T>` with reference counting, but it also allows concurrency control.
  * The members of T cannot be directly access with the smart pointer, instead a behaviour needs to be asynchronosly scheduled on the object if access is desired.
  * We will call this the concurrent owner of an instance of T.
* `when(verona::cpp::cown_ptr<T1>, verona::cpp::cown_ptr<T2>, ...) << [...](verona::cpp::acquired_cown<T1>, verona::cpp::acquired_cown<T2>) { }`.
  * This operation schedules a behaviour (represented by a C++ lambda) onto the set of concurrent owners.
  * The lambda will execute `when` no other behaviour is executing on any of the listed concurrent owners.
  * The lambda has receives `verona::cpp::acquired_cown<T>` arguments, which allow access to the underlying instance of type T.
  * `verona::cpp::acquired_cown<T>` can neither be copied or moved, lvalue references can be used to pass it into functions.
  * If two calls to `when` happen within the same behaviour (or in the original main), then there will be a happens-before relationship if there is an intersetion in the sets of concurrent owners.

## Caveats

Traditional synchronization primitives like mutexes and condition variables should not be used together with Verona behaviours.
Behaviours are expected to run until completion and taking spinlocks or sleeping the core can deadlock the system as no other behaviour can be scheduled on that core.
