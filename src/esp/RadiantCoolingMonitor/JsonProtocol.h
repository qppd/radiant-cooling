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

struct IncomingMessage {
  uint8_t version = 0;
  MsgType type    = MsgType::Status;
  String  src;
  uint32_t seq    = 0;
  JsonDocument data;
};

class JsonProtocol {
public:
  static size_t encode(MsgType type, const char* src, uint32_t seq,
                       const JsonDocument& payload, char* out, size_t maxLen);

  static bool decode(const char* json, size_t len, IncomingMessage& msg);
};
