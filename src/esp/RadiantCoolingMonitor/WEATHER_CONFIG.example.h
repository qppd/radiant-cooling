/*
 * WEATHER_CONFIG.example.h - template for WeatherAPI.com credentials
 *
 * 1. Copy this file to WEATHER_CONFIG.h   (same folder)
 * 2. Fill in your API key and location
 * 3. Upload the sketch - WEATHER_CONFIG.h is git-ignored, so the real key
 *    stays local and is never committed.
 *
 * Get a free API key at https://www.weatherapi.com
 */
#pragma once
#include <Arduino.h>

static const char WEATHER_API_KEY[]  = "your-weatherapi-key";
static const char WEATHER_LOCATION[] = "your-city";
