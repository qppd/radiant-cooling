# WeatherAPI.com

> Free-tier weather API used by the Flutter app to fetch the outdoor
> temperature and **dew point** that drive the chiller pump control.

## Key facts

- **Current weather endpoint:**
  `GET https://api.weatherapi.com/v1/current.json?key=<API_KEY>&q=<LOCATION>`
- The `current` object includes `temp_c`, `humidity`, and — critically —
  **`dewpoint_c`** (outdoor dew point in °C).
- Free tier has a **daily/monthly call budget** — keep the poll interval
  conservative (the app polls every 15 min by default).
- `q` can be a city name, lat/lon, or postal code.

## How it is used here

- The **Flutter app** owns the API key (`lib/config/app_config.dart`,
  git-ignored; copy from `app_config.example.dart`) — the ESP32 never
  stores or calls the API.
- The app polls `current.json`, parses `current.temp_c`, `current.humidity`
  and `current.dewpoint_c`, and writes
  `{ temp_c, dewpoint_c, humidity_pct, ts }` to
  `radiant/config/weather` in Firebase.
- The gateway **streams** that node and feeds the values into
  `ClimateControl`; stale data (no update within `WEATHER_STALE_S`, default
  1 h) is ignored and the condensation floor falls back to the indoor DHT22
  dew point alone.
- The outdoor dew point is combined with the indoor dew point (DHT22) —
  reference = the **higher** of the two — to set the anti-condensation water
  floor in `ClimateControl`.

## Links

- Docs: <https://www.weatherapi.com/docs/>
- Pricing / free tier: <https://www.weatherapi.com/pricing.aspx>
