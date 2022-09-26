// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <memory>
#include <span>
#include <type_traits>

namespace monza
{
  /**
   * Implements a properly length tracked C-style array wrapped in
   * std::unique_ptr or std::shared_ptr. There is no alternative in STL for a
   * fixed run-time sized array with a managed lifetime.
   */
  template<typename SmartPtrT>
  class SmartArray
  {
  public:
    using value_type = typename SmartPtrT::element_type;

  private:
    size_t length;
    SmartPtrT impl;

  public:
    /**
     * Explicit copy constructor since explicit move constructor is needed.
     * Restrict to SmartPtrT variants that are copy constructible.
     */
    SmartArray(const SmartArray& other) requires std::is_copy_constructible_v<
      SmartPtrT> = default;

    /**
     * Explicit move constructor to 0 out the length.
     */
    SmartArray(SmartArray&& other)
    : length(other.length), impl(std::move(other.impl))
    {
      other.length = 0;
    }

    /**
     * Explicit copy assignment since explicit move constructor is needed.
     * Restrict to SmartPtrT variants that are copy assignable.
     */
    SmartArray& operator=(
      const SmartArray& other) requires std::is_copy_assignable_v<SmartPtrT> =
      default;

    /**
     * Explicit move assignment to 0 out the length.
     */
    SmartArray& operator=(SmartArray&& other)
    {
      impl = std::move(other.impl);
      auto temp_length = other.length;
      other.length = 0;
      length = temp_length;

      return *this;
    }

    /**
     * Create empty array.
     */
    constexpr SmartArray() : length(0), impl(nullptr) {}

    /**
     * Create empty array with given size.
     */
    SmartArray(size_t size) : length(size), impl(new value_type[length]) {}

    /**
     * Create the array to be a copy of a given span.
     */
    SmartArray(std::span<const value_type> view)
    : length(view.size()),
      impl(static_cast<value_type*>(
        std::aligned_alloc(alignof(value_type), sizeof(value_type) * length)))
    {
      size_t i = 0;
      for (const auto& entry : view)
      {
        new (&(impl.get()[i])) value_type(entry);
        ++i;
      }
    }

    /**
     * Create the array from an initializer list.
     */
    SmartArray(std::initializer_list<value_type> list)
    : length(list.size()),
      impl(static_cast<value_type*>(
        std::aligned_alloc(alignof(value_type), sizeof(value_type) * length)))
    {
      size_t i = 0;
      for (const auto& entry : list)
      {
        new (&(impl.get()[i])) value_type(entry);
        ++i;
      }
    }

    /**
     * STL standard size accessor.
     */
    inline constexpr size_t size() const noexcept
    {
      return length;
    }

    // No STL standard data accessor to promote the usage of spans instead.

    /**
     * Indexing operator for mutable array.
     */
    inline constexpr value_type& operator[](size_t index)
    {
      return impl.get()[index];
    }

    /**
     * Indexing operator for read-only array.
     */
    inline constexpr const value_type& operator[](size_t index) const
    {
      return impl.get()[index];
    }

    /**
     * Easy conversion to std::span to get a writeable view into the array.
     */
    inline constexpr operator std::span<value_type>() noexcept
    {
      return std::span(impl.get(), length);
    };

    /**
     * Easy conversion to std::span to get a read-only view into the array.
     */
    inline constexpr operator std::span<const value_type>() const noexcept
    {
      return std::span(impl.get(), length);
    };
  };

  template<typename T>
  using SharedArray = SmartArray<std::shared_ptr<T[]>>;

  template<typename T>
  using UniqueArray = SmartArray<std::unique_ptr<T[]>>;

  /**
   * The following are helpers to build a generic to_span which works both for
   * currently spanneable types as well as these types wrapped in
   * std::shared_ptr and std::unique_ptr.
   */

  template<class T>
  struct is_shared_ptr : std::false_type
  {};

  template<class T>
  struct is_shared_ptr<std::shared_ptr<T>> : std::true_type
  {};

  template<class T>
  struct is_unique_ptr : std::false_type
  {};

  template<class T>
  struct is_unique_ptr<std::unique_ptr<T>> : std::true_type
  {};

  template<typename T>
  auto to_span(T& object) noexcept
  {
    if constexpr (is_shared_ptr<T>::value || is_unique_ptr<T>::value)
    {
      return to_span(*object);
    }
    else
    {
      return std::span<typename T::value_type>(object);
    }
  }

  template<typename T>
  auto to_span(const T& object) noexcept
  {
    if constexpr (is_shared_ptr<T>::value || is_unique_ptr<T>::value)
    {
      return to_span(*object);
    }
    else
    {
      return std::span<const typename T::value_type>(object);
    }
  }

  template<typename T, size_t N>
  auto to_span(T (&object)[N]) noexcept
  {
    return std::span<T>(object);
  }

  template<typename T, size_t N>
  auto to_span(const T (&object)[N]) noexcept
  {
    return std::span<const T>(object);
  }

  template<typename T>
  auto to_byte_span(T& object) noexcept
  {
    if constexpr (is_shared_ptr<T>::value || is_unique_ptr<T>::value)
    {
      return to_span(*object);
    }
    else if constexpr (std::is_convertible<T, std::span<char>>::value)
    {
      return static_cast<std::span<char>>(object);
    }
    else
    {
      auto original_span = std::span<typename T::value_type>(object);
      return std::span<char>(
        reinterpret_cast<char*>(original_span.data()),
        original_span.size() * sizeof(typename T::value_type));
    }
  }

  template<typename T>
  auto to_byte_span(const T& object) noexcept
  {
    if constexpr (is_shared_ptr<T>::value || is_unique_ptr<T>::value)
    {
      return to_byte_span(*object);
    }
    else if constexpr (std::is_convertible<T, std::span<const char>>::value)
    {
      return static_cast<std::span<const char>>(object);
    }
    else
    {
      auto original_span = std::span<const typename T::value_type>(object);
      return std::span<const char>(
        reinterpret_cast<const char*>(original_span.data()),
        original_span.size() * sizeof(typename T::value_type));
    }
  }

  template<typename T, size_t N>
  auto to_byte_span(T (&object)[N]) noexcept
  {
    return std::span<char, N * sizeof(T)>(reinterpret_cast<uint8_t>(object));
  }

  template<typename T, size_t N>
  auto to_byte_span(const T (&object)[N]) noexcept
  {
    return std::span<const char, N * sizeof(T)>(
      reinterpret_cast<uint8_t>(object));
  }
}
