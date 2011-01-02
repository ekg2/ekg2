#!/bin/sh
#

find ekg -name '*.c' -exec contrib/ekg_hash_benchmark_find.sed {} \; > contrib/ekg_hash_benchmark.inc_new
find plugins -name '*.c' -exec contrib/ekg_hash_benchmark_find.sed {} \; >> contrib/ekg_hash_benchmark.inc_new
#find remote -name '*.c' -exec contrib/ekg_hash_benchmark_find.sed {} \; >> contrib/ekg_hash_benchmark.inc_new

