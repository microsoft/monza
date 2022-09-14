// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <logging.h>
#include <tcb.h>
#include <writebuffers.h>

namespace monza
{
  void compartment_callback(size_t index, void* ret, void* data);

  template<typename F>
  class Callback;

  /**
   * Handle to a callback that can be used from a compartment.
   * Only contains index into callback table of the compartment.
   */
  template<typename R, typename A>
  class CompartmentCallback
  {
  private:
    CompartmentOwner owner = CompartmentOwner::null();
    size_t index = static_cast<size_t>(-1l);

  public:
    CompartmentCallback() {}

    /**
     * Called from within the compartment.
     * Bundles argument and preprates space for return and issues
     * compartment_callback.
     */
    template<typename... Args>
    R operator()(Args&&... args) const
    {
      // Check if the callback belongs to the right compartment.
      // Not a security check as compartment can always fake it, but used to
      // catch bugs.
      if (!matches_compartment(owner))
      {
        LOG_MOD(ERROR, Compartment)
          << "Using callback with wrong compartment." << LOG_ENDL;
        abort();
      }

      auto fused_args = A(std::forward_as_tuple(args...));
      R return_value;

      compartment_callback(index, &return_value, &fused_args);

      return std::move(return_value);
    }

  private:
    CompartmentCallback(CompartmentOwner owner, size_t index)
    : owner(owner), index(index)
    {}

    template<typename F>
    friend class Callback;
  };

  /**
   * Type of the callback object for the stdout implementation from within
   * compartments. Explicitly named as a thread-local copy is set up for each
   * compartment to use.
   */
  using StdoutCallback = CompartmentCallback<size_t, std::tuple<WriteBuffers>>;
}
