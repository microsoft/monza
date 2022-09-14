// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#if defined(MONZA_COMPARTMENT_NAMESPACE)

#  define snmalloc monza_compartment_snmalloc

#else

#  define snmalloc monza_snmalloc

#endif

// Core implementation of snmalloc independent of the configuration mode
// gives enough to define a MonzaPal
#include <climits>
#include <snmalloc/aal/aal.h>
#include <snmalloc/ds_core/ds_core.h>
#include <snmalloc/pal/pal_consts.h>