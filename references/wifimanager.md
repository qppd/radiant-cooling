# WiFiManager (tzapu)

> Captive-portal WiFi provisioning library — lets the gateway board connect
> to any network **without hardcoded SSID/password**.

## Key facts

- Library: **WiFiManager** by tzapu (maintained by tablatronix); install via
  Arduino Library Manager.
- On boot it tries the **saved credentials**; if none exist (or the network
  is unreachable) it starts a **captive portal** AP the user joins from a
  phone, enters SSID/password, and the values are saved to flash.
- Credentials can be erased at runtime with `resetSettings()`.

### API used in this project (`WifiProvisioner` module)

```cpp
#include <WiFiManager.h>
WiFiManager wm;

bool connected = wm.autoConnect("AP-Name");   // returns true when joined
wm.resetSettings();                           // erase saved SSID/password
wm.setConfigPortalTimeout(180);               // portal timeout (s)
wm.setConnectTimeout(15);                     // connection timeout (s)
```

## How it is used here

- `WifiProvisioner` (gateway only) wraps WiFiManager.
- On first boot the portal AP **`RadiantCooling-AP`** opens.
- **Reset button (GPIO 33):** hold ~3 s → `resetSettings()` + `ESP.restart()`;
  holding it at power-up also erases credentials so the portal re-opens.
- **Warning:** The gateway's Wi-Fi channel becomes the router's channel — ESP-NOW peers
  must match it (fix the router to 1/6/11, see `docs/schematic/pin-map.md`).

## Links

- GitHub: <https://github.com/tzapu/WiFiManager>
- RNT — ESP32 WiFiManager: <https://randomnerdtutorials.com/esp32-wifimanager-asyncweb-mongoose/configuration-manage-network/>
- RNT — reset WiFiManager settings with a button: <https://randomnerdtutorials.com/esp32-wifimanager-asyncweb-mongoose/reset-settings-button/>
