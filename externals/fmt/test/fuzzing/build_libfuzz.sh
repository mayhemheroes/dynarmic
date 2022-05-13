#!/bin/sh
#
# Creates fuzzer builds of various kinds
# - oss-fuzz emulated mode (makes sure a simulated invocation by oss-fuzz works)
# - libFuzzer build (you will need clang)
# - afl build (you will need afl)
#
#
# Copyright (c) 2019 Paul Dreik
#
# For the license information refer to format.h.

set -e
me=$(basename $0)
root=$(readlink -f "$(dirname "$0")/../..")


echo $me: root=$root

here=$(pwd)

CXXFLAGSALL="-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION= -g"
CMAKEFLAGSALL="$root -GNinja -DCMAKE_BUILD_TYPE=Debug -DFMT_DOC=Off -DFMT_TEST=Off -DFMT_FUZZ=On -DCMAKE_CXX_STANDARD=17"

#CLANG=clang++-11

CLANG=clang++

# Builds fuzzers for local fuzzing with libfuzzer with asan+usan.
builddir=$here/build-fuzzers-libfuzzer
mkdir -p $builddir
cd $builddir
CXX=$CLANG \
CXXFLAGS="$CXXFLAGSALL -fsanitize=fuzzer-no-link,address,undefined" cmake \
cmake $CMAKEFLAGSALL \
-DFMT_FUZZ_LINKMAIN=Off \
-DFMT_FUZZ_LDFLAGS="-fsanitize=fuzzer"

#chdir $here
#mkdir -p build-fuzzers-libfuzzer

#cd build-fuzzers-libfuzzer \
#export CXX=clang++ \
#export CXXFLAGS="-fsanitize=fuzzer-no-link -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION= -g" \
#cmake .. -DFMT_SAFE_DURATION_CAST=On -DFMT_FUZZ=On -DFMT_FUZZ_LINKMAIN=Off -DFMT_FUZZ_LDFLAGS="-fsanitize=fuzzer" \
#cmake --build .

cmake --build $builddir

echo $me: all good
