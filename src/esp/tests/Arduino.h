/*
 * Arduino.h - minimal host stand-in for the Arduino core header.
 *
 * Lets the pure-math ClimateControl module compile and run on a desktop
 * machine (no Arduino toolchain needed). ClimateControl only relies on
 * standard types and <math.h>, so this stub is all it takes.
 *
 * NOTE: this file lives under src/esp/tests/ and is NOT part of any
 * sketch - Arduino IDE never compiles it.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <math.h>
