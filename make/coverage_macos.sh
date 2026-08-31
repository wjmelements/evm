#!/bin/bash
set -e
LLVM_COV=${LLVM_COV:-/usr/local/opt/llvm/bin/llvm-cov}
LLVM_PROFDATA=${LLVM_PROFDATA:-/usr/local/opt/llvm/bin/llvm-profdata}
COVFLAGS="-O0 -fprofile-instr-generate -fcoverage-mapping -fdiagnostics-color=auto -Wno-multichar -pthread -std=gnu11"

make again CC=clang CFLAGS="$COVFLAGS"
rm -rf cov/
mkdir -p cov
LLVM_PROFILE_FILE="cov/evm-%p.profraw" make check CC=clang CFLAGS="$COVFLAGS"
$LLVM_PROFDATA merge -sparse cov/*.profraw -o cov/evm.profdata
$LLVM_COV show bin/evm -instr-profile=cov/evm.profdata -format=html -output-dir=cov/ src/
echo "Coverage report: cov/index.html"
