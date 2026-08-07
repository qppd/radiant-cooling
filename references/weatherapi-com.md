# WeatherAPI.com

> Free-tier weather API used by the gateway to fetch the outdoor temperature
> and **dew point** that drive the chiller pump control.

## Key facts

- **Current weather endpoint:**
  `GET https://api.weatherapi.com/v1/current.json?key=<API_KEY>&q=<LOCATION>`
- The `current` object includes `temp_c`, `humidity`, and — critically —
  **`dewpoint_c`** (outdoor dew point in °C).
- Free tier has a **daily/monthly call budget** — keep the poll interval
  conservative (this project uses 900 s = 15 min).
- `q` can be a city name, lat/lon, or postal code.

## How it is used here

- `WeatherApi` module (gateway only) wraps `HTTPClient` + ArduinoJson:
  - builds the URL from `WEATHER_API_KEY` / `WEATHER_LOCATION`, defined in
    `WEATHER_CONFIG.h` (git-ignored; copy from `WEATHER_CONFIG.example.h`)
  - parses `current.temp_c`, `current.humidity`, `current.dewpoint_c`
  - called throttled from the main loop (`WEATHER_POLL_S`)
- The outdoor dew point is combined with the indoor dew point (DHT22) —
  reference = the **higher** of the two — to set the anti-condensation water
  floor in `ClimateControl`.

## Links

- Docs: <https://www.weatherapi.com/docs/>
- Pricing / free tier: <https://www.weatherapi.com/pricing.aspx>
