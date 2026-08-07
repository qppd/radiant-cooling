# ESP-NOW

> Espressif's connectionless peer-to-peer protocol — the wireless link
> between the three ESP32 boards in this project (no router or broker).

## Key facts

- Built into the ESP32 silicon and the Arduino ESP32 core — **no external
  library** (`#include <esp_now.h>`).
- **Max payload: 250 bytes** (ESP-NOW v1.0); newer ESP32 variants with
  ESP-NOW v2.0 support up to 1470 bytes.
- Works in **STA mode without associating** to a router, but the interface
  still occupies a Wi-Fi channel.
- **Channel lock:** all peers must operate on the same channel as the
  gateway's router connection. Peers that are not associated default to
  channel 1 → classic silent-failure mode.
- Best-effort delivery — no built-in acknowledgement (the send callback
  reports delivery status per peer).

## API used in this project (`EspNowTransport` module)

```cpp
WiFi.mode(WIFI_STA);                 // required
esp_now_init();                      // returns ESP_OK on success
esp_now_peer_info_t peer = {};
memcpy(peer.peer_addr, mac, 6);      // peer MAC
esp_now_add_peer(&peer);             // register a peer
esp_now_register_recv_cb(cb);        // void cb(const uint8_t*, const uint8_t*, int)
esp_now_register_send_cb(cb);        // void cb(const uint8_t*, esp_now_send_status_t)
esp_now_send(mac, data, len);        // queue a message to a peer
```

## How it is used here

- `RadiantCoolingMonitor` (gateway) registers both controller boards as peers.
- `WaterChillerController` and `DehumidifierController` register the gateway
  and send telemetry; they receive `set_pumps` / config commands.
- Message payloads are JSON (≤ 250 B) encoded by the `JsonProtocol` module —
  see `docs/api.md`.

## Links

- ESP-IDF ESP-NOW API reference: <https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html>
- Arduino-ESP32 ESP_NOW examples: <https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP_NOW>
- Random Nerd Tutorials — ESP32 ESP-NOW: <https://randomnerdtutorials.com/esp-now-esp32-arduino-ide/>
