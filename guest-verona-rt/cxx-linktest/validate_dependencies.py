#!/usr/bin/python3

# Copyright Microsoft and Project Monza Contributors.
# SPDX-License-Identifier: MIT

import sys

allowed_function = {
  # snmalloc malloc overrides
  "malloc",
  "calloc",
  "realloc",
  "free",
  "posix_memalign",

  # snmalloc helper dependencies
  "kabort",

  # System symbols
  "__dso_handle",

  # Monza start/stop
  "_ZN5monza10monza_exitEi",
  "_ZN5monza16monza_finalizersEv",
  "_ZN5monza18monza_initializersEv",

  # Monza complex functionality
  "_ZN5monza7get_tcbEv",
  "_ZN5monza13get_thread_idEv",
  "_ZN5monza14kwritev_stdoutENSt3__14spanIKNS1_IKhLm18446744073709551615EEELm18446744073709551615EEE",
  "_ZN5monza8Spinlock7acquireEv",
  "_ZN5monza8Spinlock7releaseEv",
  "_ZN5monza12get_tls_slotEt",
  "_ZN5monza12set_tls_slotEtPv",
  "_ZN5monza17allocate_tls_slotEPt",
  "_ZN5monza14get_alloc_sizeEPKv",
  "_ZN5monza16get_base_pointerEPv",
  "_ZN5monza11wake_threadEj",
  "_ZN5monza12sleep_threadEv",
  "_ZN5monza11init_timingERK8timespec",
  "_ZN5monza12get_timespecEb",
  "_ZN5monza16output_log_entryENSt3__14spanIKhLm18446744073709551615EEE",
  "_ZN5monza6Logger13global_streamE",
  "_ZN5monza6Logger19thread_local_streamE",
  "_ZN5monza15is_confidentialEv",
}

success = True
for line in sys.stdin:
  symbol_info = line.split()
  if len(symbol_info) != 2:
    continue
  if symbol_info[0] != "U":
    continue
  if not symbol_info[1] in allowed_function:
    error_string = f"Undefined dependency in monza C/C++ runtimes {symbol_info[1]}"
    print(error_string, file=sys.stderr)
    success = False

if not success:
  sys.exit(-1)
