// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <logging.h>

constexpr const char C_LOCALE_NAME[] = "C";

/**
 * "Create" a copy of the Monza default C locale for legacy code (in particular
 * libcxx). By only allowing the creation of this locale, we don't need to
 * actually create it as using it in setlocale and uselocale is a NOP. Error out
 * if other behaviour is requested.
 */
extern "C" locale_t
newlocale(int category_mask, const char* locale, locale_t base)
{
  if (
    category_mask != LC_ALL_MASK || strcmp(locale, C_LOCALE_NAME) != 0 ||
    base != nullptr)
  {
    LOG_MOD(ERROR, LIBC) << "Invalid argument to newlocale. Only default "
                            "LC_ALL_MASK/C/nullptr is supported in Monza."
                         << LOG_ENDL;
    abort();
  }
  return static_cast<locale_t>(malloc(0));
}

/**
 * Free a previously allocated locale object.
 * As newlocale creates an empty object of minimal size, this is just about
 * freeing said object.
 */
extern "C" void freelocale(locale_t locale)
{
  free(locale);
}

/**
 * Allow setting the localization to the Monza default C locale for legacy code
 * (in particular libcxx). Only accept the default locale or a query, so no
 * change actually needs to happen in the system state. Error out if other
 * behaviour is requested.
 */
extern "C" char* setlocale(int category, const char* locale)
{
  if (
    category != LC_ALL ||
    (locale != nullptr && strcmp(locale, C_LOCALE_NAME) != 0))
  {
    LOG_MOD(ERROR, LIBC) << "Invalid argument to setlocale. Only default "
                            "LC_ALL/(C | nullptr) is supported in Monza."
                         << LOG_ENDL;
    abort();
  }

  // The API is very badly defined as the returned string should never be
  // mutated. Workaround with const_cast and sad kittens.
  return const_cast<char*>(C_LOCALE_NAME);
}

/**
 * Change the thread-local active locale to one created previously with
 * newlocale. Since newlocale does not allow the creation of any new locale, so
 * no change actually needs to happen in the system state.
 */
extern "C" locale_t uselocale(locale_t locale)
{
  return locale;
}
