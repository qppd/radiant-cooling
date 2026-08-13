# WeatherAPI.com

> Free-tier weather API called by the **gateway (monitor board)** for the
> outdoor temperature and **dew point** that drive the chiller pump control;
> the API key is managed from the Flutter app.

## Key facts

- **Current weather endpoint:**
  `GET https://api.weatherapi.com/v1/current.json?key=<API_KEY>&q=<LOCATION>`
- The `current` object includes `temp_c`, `humidity`, and — critically —
  **`dewpoint_c`** (outdoor dew point in °C).
- Free tier has a **daily/monthly call budget** — keep the poll interval
  conservative (the gateway polls every 15 min by default).
- `q` can be a city name, lat/lon, or postal code.

## How it is used here

- The **gateway (monitor board)** calls `current.json` itself; the API key
  is **managed by the Flutter app** (`lib/config/app_config.dart`,
  git-ignored; copy from `app_config.example.dart`) and delivered at
  runtime via `radiant/config/weather_key` (Firebase) — it is never
  compiled into the firmware.
- The gateway polls every `WEATHER_POLL_S` (default 15 min), parses
  `current.temp_c`, `current.humidity` and `current.dewpoint_c`, and feeds
  them into `ClimateControl`.
- Failed/stale fetches (no valid data within `WEATHER_STALE_S`, default
  1 h) disable weather demand and fall back to the indoor DHT22 dew point
  alone.
- The outdoor dew point is combined with the indoor dew point (DHT22) —
  reference = the **higher** of the two — to set the anti-condensation water
  floor in `ClimateControl`.

## Links

- Docs: <https://www.weatherapi.com/docs/>
- Pricing / free tier: <https://www.weatherapi.com/pricing.aspx>
