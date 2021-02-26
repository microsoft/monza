// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

/**
 * These are methods used by OpenSSL, which have no meaningful implementation on
 * Monza. They are stubbed out in a way that does not interfere with correct
 * OpenSSL operations, but are kept within the OpenSSL build.
 */

/**
 * Only used to track if forking had occured to reseed the RNG.
 * Monza has no forking.
 */
#define getpid() (1)

/**
 * Used to establish if this is a setuid binary. Always false on Monza.
 */
#define getauxval(key) (0)

/**
 * Used for sleeping on polling. No meaninful time-based sleep in Monza.
 * Cooperative scheduler means that no resources would be released.
 */
#define usleep(usec) (0)
