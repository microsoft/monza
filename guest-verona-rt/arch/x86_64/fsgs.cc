// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

namespace monza
{
  void set_tls_base(void* ptr)
  {
    asm volatile("wrfsbase %0" : : "D"(ptr));
  }

  void* get_tls_base()
  {
    void* ret;
    asm volatile("rdfsbase %0" : "=a"(ret));
    return ret;
  }
}
