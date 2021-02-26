// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <cores.h>
#include <crt.h>
#include <early_alloc.h>
#include <logging.h>
#include <snmalloc.h>
#include <tls.h>

extern void (*__init_array_start)(void);
extern void (*__init_array_end)(void);
extern void (*__fini_array_start)(void);
extern void (*__fini_array_end)(void);

extern char __stack_start;
extern char __stack_end;

extern int main();
extern "C" int __libc_start_main(int (*main)());
extern "C" int __libc_start_main(int (*main)());
extern "C" void (*shutdown)();

extern "C"
{
  void monza_dummy_initfini() {}
  int __attribute__((weak, alias("monza_dummy_initfini"))) _init();
  int __attribute__((weak, alias("monza_dummy_initfini"))) _fini();
}

namespace monza
{
  extern void monza_thread_initializers();

  void monza_initializers()
  {
    monza_thread_initializers();
    _init();
    for (auto fn = &__init_array_start; fn < &__init_array_end; fn++)
    {
      (*fn)();
    }
  }

  void monza_finalizers()
  {
    for (auto fn = &__fini_array_end - 1; fn >= &__fini_array_start; fn--)
    {
      (*fn)();
    }
    _fini();
  }

  [[noreturn]] void monza_exit(int status)
  {
    LOG(CRITICAL) << "Execution finished with " << status << "." << LOG_ENDL;
    shutdown();
    // Needed for no-exit and virtual shutdown.
    while (true)
      ;
  }

  [[noreturn]] void monza_main()
  {
    executing_cores.store(1, std::memory_order_release);

    void* main_thread_tls = create_tls(true, &__stack_start, &__stack_end);
    get_thread_execution_context(0).tls_ptr = main_thread_tls;
    set_tls_base(main_thread_tls);

    int ret = __libc_start_main(main);

    executing_cores.fetch_add((size_t)-1, std::memory_order_release);
    while (executing_cores.load(std::memory_order_acquire) > 0)
    {
      snmalloc::Aal::pause();
    }

    monza_exit(ret);
  }

  extern "C" [[noreturn]] void kabort()
  {
    monza::monza_exit(127);
  }
}
