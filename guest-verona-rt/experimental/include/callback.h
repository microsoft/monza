// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <compartment_callback.h>
#include <cstddef>
#include <functional>
#include <snmalloc.h>
#include <tcb.h>

namespace monza
{
  class CompartmentBase;
  class CallbackBase;

  extern "C" void abort_kernel_callback(int status);
  extern "C" void
  compartment_forward_callback(CompartmentBase*, size_t, void*, void*);

  /**
   * Abstract class for a callback to allow storing pointers to diversly
   * templated subclasses. Virtual callback method called by
   * compartment_forward_invoke when back in the kernel to dispatch the
   * callback.
   */
  class CallbackBase
  {
  protected:
    virtual ~CallbackBase() {}

    virtual void callback(CompartmentOwner, void*, void*) const = 0;

    friend class CompartmentBase;
    friend void
    compartment_forward_callback(CompartmentBase*, size_t, void*, void*);
  };

  /**
   * Typed callback class used to store registered callbacks within
   * compartments. Extends the abstract class CallbackBase, implementing the
   * dynamically dispatched callback interface.
   */
  template<typename F>
  class Callback : public CallbackBase
  {
    F f;

    /**
     * Type tricks to extract argument and return types from a function type.
     */
    template<typename T>
    struct function_types;

    template<typename R, typename... Args>
    struct function_types<std::function<R(Args...)>>
    {
      using result_type = R;
      using args_type = std::tuple<Args...>;
    };

    using T = function_types<decltype(std::function(f))>;
    using R = typename T::result_type;
    using A = typename T::args_type;

  private:
    Callback(F f) : CallbackBase(), f(f) {}

    /**
     * Create a typed CompartmentCallback handle from the callback.
     * Practically a static method, but uses the typing within a particular
     * instance.
     */
    auto get_compartment_callback(CompartmentOwner owner, size_t index)
    {
      return CompartmentCallback<R, A>(owner, index);
    }

    /**
     * Called by compartment_forward_invoke when back in the kernel to dispatch
     * the callback.
     */
    virtual void callback(CompartmentOwner owner, void* ret, void* data) const
    {
      auto typed_data = reinterpret_cast<A*>(data);
      auto typed_ret = reinterpret_cast<R*>(ret);
      if (!snmalloc::MonzaCompartmentOwnership::validate_owner(
            owner, typed_data))
      {
        LOG_MOD(ERROR, Compartment)
          << "Callback argument pointer not owned by compartment." << LOG_ENDL;
        abort_kernel_callback(-1);
      }
      if (!snmalloc::MonzaCompartmentOwnership::validate_owner(
            owner, typed_ret))
      {
        LOG_MOD(ERROR, Compartment)
          << "Callback result pointer not owned by compartment." << LOG_ENDL;
        abort_kernel_callback(-1);
      }
      *typed_ret = std::apply(f, *typed_data);
    }

    friend class CompartmentBase;
  };
}
