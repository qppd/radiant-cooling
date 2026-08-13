# Firmware Unit Tests

Host-machine tests for the pure-math modules of the ESP32 firmware.

## What is tested

| File                     | Module                                        | Covers                                             |
| ------------------------ | --------------------------------------------- | -------------------------------------------------- |
| `test_climate_control.cpp` | `ClimateControl` (gateway)                   | Magnus dew point + pump decision logic (DHT22 demand, coldest-pipe/tank condensation floor, hysteresis, fail-safe, reference selection) |

The `ClimateControl` module has no Arduino hardware dependencies, so it is
compiled against the stub `Arduino.h` in this folder and run as a normal
desktop program (no Arduino toolchain required).

## Requirements

- `g++` (Linux, macOS, or Git Bash / WSL on Windows)

## Run

```bash
cd src/esp/tests
./run_tests.sh
```

Exit code `0` = all checks passed.

## Adding tests

- Add a test function in the relevant `test_*.cpp`, call it from `main()`,
  and use `CHECK(...)` / `CHECK_CLOSE(actual, expected, tol)`.
- Add more modules here the same way — the stub `Arduino.h` only needs to
  be extended if a module under test uses additional Arduino core APIs.
