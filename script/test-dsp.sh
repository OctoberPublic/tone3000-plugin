#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [ ! -f build/CMakeCache.txt ]; then
  cmake --preset linux-dev
fi
cmake --build build --target DspTests --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
# The suite now also exercises the offline editor. A virtual X display is
# required on Linux even though the test runner itself is a console binary.
if [[ $(uname -s) == Linux ]]; then
  exec xvfb-run -a build/test/DspTests_artefacts/Release/DspTests ${1:+--gtest_filter="$1"}
fi
exec build/test/DspTests_artefacts/Release/DspTests ${1:+--gtest_filter="$1"}
