#!/usr/bin/python3

# Copyright Microsoft and Project Monza Contributors.
# SPDX-License-Identifier: MIT

import gdb
import os

app = os.getenv('MONZA_QEMU_APP')

gdb.execute('target remote localhost:1234')
gdb.execute(f'symbol-file {app}')
