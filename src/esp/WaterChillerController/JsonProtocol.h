/*
 * JsonProtocol.h - protocol module
 *
 * Wraps the ArduinoJson library. Encodes/decodes the ESP-NOW message
 * envelope defined in docs/api.md:
 *   { "v": 1, "t": <type>, "src": <id>, "seq": <n>, "d": { ... } }
 */
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdint.h>

enum class MsgType : uint8_t {
  Telemetry = 1,
  State     = 2,
  Cmd       = 3,
  Config    = 4,
  Status    = 5,
};

// NOTE: contains a move-only JsonDocument - pass IncomingMessage by
// reference, never copy it.
struct IncomingMessage {
  uint8_t version = 0;
  MsgType type    = MsgType::Status;
  String  src;
  uint32_t seq    = 0;
  JsonDocument data;          // decoded "d" object
};

class JsonProtocol {
public:
  // Encodes a message into `out`; returns bytes written (0 on error).
  static size_t encode(MsgType type, const char* src, uint32_t seq,
                       const JsonDocument& payload, char* out, size_t maxLen);

  // Decodes a message; returns true on success.
  static bool decode(const char* json, size_t len, IncomingMessage& msg);
};
