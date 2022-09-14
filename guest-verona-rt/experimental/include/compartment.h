// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <arch_compartment.h>
#include <callback.h>
#include <cstddef>
#include <cstdint>
#include <new>
#include <snmalloc.h>
#include <writebuffers.h>

extern size_t __stack_size;

namespace monza
{
  class CompartmentBase;

  extern "C" bool compartment_enter(
    void* lambda,
    void* ret,
    void* data,
    void (*fp)(void*, void*, void*),
    void* sp,
    CompartmentBase* self);
  void compartment_return();
  extern "C" void compartment_forward_exit(CompartmentBase*, int);
  extern "C" void*
  compartment_forward_alloc_chunk(CompartmentBase*, size_t, uintptr_t);
  extern "C" void
  compartment_forward_dealloc_chunk(CompartmentBase*, void* base, size_t);
  extern "C" void*
  compartment_forward_alloc_meta_data(CompartmentBase*, size_t);

  void monza_thread_initializers();
  void snmalloc_compartment_initializer(void* backend_state);

  class CompartmentBase : public ArchitecturalCompartmentBase
  {
  protected:
    using WriterFunction = size_t (*)(CompartmentOwner, WriteBuffers);

    bool is_valid = true;
    std::vector<CallbackBase*> callbacks;

    CompartmentBase() {}

    ~CompartmentBase()
    {
      for (auto callback : callbacks)
      {
        delete callback;
      }
    }

    void* get_root_pagemap()
    {
      return &snmalloc::MonzaGlobals::Pagemap::concretePagemap;
    }

    static void setup_stdout(StdoutCallback);

    static WriterFunction get_compartment_writer();

  public:
    bool check_valid() const
    {
      return is_valid;
    }

    template<typename F>
    auto register_callback(F f)
    {
      auto index = callbacks.size();
      auto callback = new Callback(f);
      callbacks.push_back(callback);
      return callback->get_compartment_callback(get_owner(), index);
    }

    const CallbackBase* get_callback(size_t index) const
    {
      if (index < callbacks.size())
      {
        return callbacks[index];
      }
      else
      {
        return nullptr;
      }
    }

  private:
    void invalidate(int)
    {
      is_valid = false;
    }

    // Friend function so that it can use private members.
    friend void compartment_forward_exit(CompartmentBase*, int);
    friend void*
    compartment_forward_alloc_chunk(CompartmentBase*, size_t, uintptr_t);
    friend void
    compartment_forward_dealloc_chunk(CompartmentBase*, void*, size_t);

    friend void* compartment_forward_alloc_meta_data(CompartmentBase*, size_t);
  };

  class InvokeScopedStack
  {
    CompartmentBase& compartment;
    std::span<uint8_t> range;
    size_t reserved;
    static constexpr size_t STACK_ALIGNMENT = 16;

  public:
    InvokeScopedStack(CompartmentBase& compartment)
    : compartment(compartment), range(compartment.get_stack()), reserved(0)
    {}

    ~InvokeScopedStack()
    {
      compartment.release_stack();
    }

    template<typename T>
    T* reserve()
    {
      reserved += sizeof(T);
      reserved = snmalloc::bits::align_up(reserved, alignof(T));
      uint8_t* ret = range.last(reserved).data();
      compartment.update_active_stack_usage(snmalloc::address_cast(ret));
      return reinterpret_cast<T*>(ret);
    }

    uint8_t* get()
    {
      return snmalloc::pointer_align_down<uint8_t>(
        range.last(reserved).data(), STACK_ALIGNMENT);
    }
  };

  template<typename FData = NoData>
  class Compartment : public CompartmentBase
  {
  private:
    CompartmentMemory<FData, false, true> data;

  public:
    inline Compartment() : CompartmentBase(), data(alloc_local_state)
    {
      auto p = get_root_pagemap();

      auto stdout_callback = register_callback(
        [owner = get_owner(), writer = get_compartment_writer()](
          WriteBuffers buffers) { return writer(owner, buffers); });

      if constexpr (std::is_same_v<FData, NoData>)
      {
        invoke([p, stdout_callback]() {
          setup_stdout(stdout_callback);
          snmalloc_compartment_initializer(p);
          monza_thread_initializers();
          return true;
        });
      }
      else
      {
        invoke([p, stdout_callback](FData*) {
          setup_stdout(stdout_callback);
          snmalloc_compartment_initializer(p);
          monza_thread_initializers();
          return true;
        });
      }
    }

    inline ~Compartment() {}

    Compartment(const Compartment&) = delete;
    Compartment(Compartment&&) = delete;

    template<typename F>
    auto invoke(F lambda)
    {
      if constexpr (std::is_same_v<FData, NoData>)
      {
        using FRet = decltype(lambda());
        return invoke_typed<F, FRet>(lambda);
      }
      else
      {
        using FRet = decltype(lambda(data.get_ptr()));
        return invoke_typed<F, FRet>(lambda);
      }
    }

    FData& get_data()
    {
      return *(data.get_ptr());
    }

    template<typename CData, bool clear = true, bool zero = false>
    CompartmentMemory<CData, clear, zero> alloc_compartment_memory(size_t count)
    {
      auto mem =
        CompartmentMemory<CData, clear, zero>(alloc_local_state, count);
      return mem;
    }

  private:
    template<typename F, typename FRet>
    inline CompartmentErrorOr<FRet> invoke_typed(F lambda)
    {
      if (!is_valid)
      {
        return CompartmentErrorOr<FRet>();
      }

      InvokeScopedStack stack(*this);

      FRet* compartment_ret = stack.reserve<FRet>();

      bool ret = compartment_enter(
        &lambda,
        compartment_ret,
        data.get_ptr(),
        &invoke_helper<F, FRet>,
        stack.get(),
        this);

      if (!ret)
      {
        return CompartmentErrorOr<FRet>();
      }

      return CompartmentErrorOr<FRet>(std::move(*compartment_ret));
    }

    template<typename F, typename FRet>
    /**
     * This runs inside the compartment, unwrapping the lambda, executing it and
     *wrapping the return into the target storage area.
     **/
    static void invoke_helper(
      void* lambda_ptr_raw, void* return_ptr_raw, void* data_ptr_raw)
    {
      F* lambda_ptr = static_cast<F*>(lambda_ptr_raw);
      FRet* return_ptr = static_cast<FRet*>(return_ptr_raw);
      if constexpr (std::is_same_v<FData, NoData>)
      {
        new (return_ptr) FRet(std::move(*lambda_ptr)());
      }
      else
      {
        new (return_ptr)
          FRet(std::move(*lambda_ptr)(static_cast<FData*>(data_ptr_raw)));
      }
      compartment_return();
    }
  };
}
