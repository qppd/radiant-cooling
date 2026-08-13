#!/usr/bin/env bash
# Compile and run the ClimateControl unit tests on a host machine.
# Requires g++ (any POSIX-ish compiler). Works in Git Bash on Windows too.
set -euo pipefail
cd "$(dirname "$0")"

BIN="${TMPDIR:-/tmp}/radiant_climate_control_tests"

echo "==> Compiling ClimateControl tests..."
g++ -std=c++11 -Wall -Wextra \
    -I. -I../RadiantCoolingMonitor \
    test_climate_control.cpp \
    ../RadiantCoolingMonitor/ClimateControl.cpp \
    -o "$BIN" -lm

echo "==> Running tests..."
"$BIN"
