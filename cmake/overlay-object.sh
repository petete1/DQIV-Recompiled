#!/bin/sh
# Compiler launcher for the game's modules (GCC): compiles one source, then
# moves its writable data, zeroed data and constructors into the sections of
# its DS overlay (dqiv_d<id>, dqiv_b<id>, dqiv_i<id>). The linker bounds each
# with __start_/__stop_ symbols; constructors no longer run at startup.
# Usage: overlay-object.sh <readelf> <objcopy> <compiler> <arguments...>
readelf=$1 objcopy=$2
shift 2
"$@" || exit
id= output= previous=
for argument in "$@"; do
    case $argument in -DPC_OVERLAY_MODULE=*) id=${argument#-DPC_OVERLAY_MODULE=} ;; esac
    [ "$previous" = -o ] && output=$argument
    previous=$argument
done
[ -n "$id" ] && [ -n "$output" ] || exit 0
renames=$("$readelf" -SW "$output" | sed -n 's/^ *\[ *[0-9]*\] \([^ ]*\) .*/\1/p' | awk -v id="$id" '
    /^\.data($|\.)/ && !/^\.data\.rel\.ro/ {print "--rename-section " $1 "=dqiv_d" id}
    /^\.bss($|\.)/ {print "--rename-section " $1 "=dqiv_b" id}
    /^\.init_array($|\.)/ {print "--rename-section " $1 "=dqiv_i" id}
    /^\.ctors($|\.)/ {print "unsupported " $1; exit 1}')
case $renames in *unsupported*) echo "$output: $renames" >&2; exit 1 ;; esac
[ -z "$renames" ] || exec "$objcopy" $renames "$output"
