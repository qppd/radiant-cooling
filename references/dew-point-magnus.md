# Dew Point — Magnus Formula

> The indoor dew point is computed from the dehumidifier's DHT22
> (temperature + relative humidity) using the Magnus formula.

## Formula

```
α  = (a · T) / (b + T) + ln(RH / 100)
Td = (b · α) / (a − α)
```

with:

- `T` — air temperature (°C)
- `RH` — relative humidity (%)
- `a = 17.62`, `b = 243.12 °C` (constants for 0..60 °C range)

Guarded against `RH ≤ 0` (log(0) → NaN) — invalid readings yield no indoor
dew point and the outdoor (weather) value is used alone.

## Why it matters here

Condensation forms when a surface is colder than the dew point of the air
touching it. Radiant cooling keeps the chilled-water supply above
`dew point + margin`:

```
refDewPoint = max(outdoorDewPoint (WeatherAPI via the app), indoorDewPoint (DHT22))
waterFloor  = refDewPoint + dewpoint_margin_c
```

Implemented in the `ClimateControl` module (`dewPointC()` and `decidePumps()`).

## Links

- Magnus formula (Dew point article): <https://en.wikipedia.org/wiki/Dew_point>
- Arden Buck equation (similar approximation): <https://en.wikipedia.org/wiki/Arden_Buck_equation>
