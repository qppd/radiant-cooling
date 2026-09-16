#!/usr/bin/env bash
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
