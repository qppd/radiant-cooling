#include "JsonProtocol.h"

size_t JsonProtocol::encode(MsgType type, const char* src, uint32_t seq,
                            const JsonDocument& payload,
                            char* out, size_t maxLen) {
  JsonDocument doc;
  doc["v"]   = 1;
  doc["t"]   = static_cast<int>(type);
  doc["src"] = src;
  doc["seq"] = seq;
  doc["d"]   = payload;
  return serializeJson(doc, out, maxLen);
}

bool JsonProtocol::decode(const char* json, size_t len, IncomingMessage& msg) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len) != DeserializationError::Ok) return false;

  const int t = doc["t"] | 0;
  if (t < static_cast<int>(MsgType::Telemetry) || t > static_cast<int>(MsgType::Status)) {
    return false;                        // unknown message type
  }
  msg.version = doc["v"] | 0;
  msg.type    = static_cast<MsgType>(t);
  msg.src     = doc["src"] | "";
  msg.seq     = doc["seq"] | 0U;
  msg.data.set(doc["d"]);
  return msg.version >= 1;
}
