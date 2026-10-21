#!/usr/bin/env bash
# Usage: bash Tools/CI/build.sh [Debug|Release]    (default: Debug)
set -e                                  # stop if any step fails
cd "$(dirname "$0")/../.."              # always run from the repo root
CONFIG=${1:-Debug}                      # $1 takes first arg, if arg empty use Debug

cmake -S . -B build -DCMAKE_BUILD_TYPE="$CONFIG"      # configure
cmake --build build --config "$CONFIG"                # compile
ctest --test-dir build --output-on-failure            # run all tests

echo "Success! All tests passed [$CONFIG]."
