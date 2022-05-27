#!/usr/bin/env bash

STATUS=0

files=`git ls-files -- ':!:external/*' '*.cpp' '*.cc' '*.h' '*.hh' '*.asm' 's' '.py' | xargs`
grep -L "Copyright Microsoft and Project Monza Contributors."  $files > header_missing
if [ -s header_missing ]; then
  echo "Copyright missing on:"
  cat header_missing
  STATUS=1
fi

grep -L "SPDX-License-Identifier: MIT" $files > header_missing
if [ -s header_missing ]; then
  echo "License missing on:"
  cat header_missing
  STATUS=1
fi

rm -f header_missing

exit $STATUS
