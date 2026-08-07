#include "FirebaseSync.h"

// TODO: implement against the Mobizt FirebaseClient library
//   (library: "FirebaseClient" in Arduino Library Manager).
//
// Reference the library's async examples for:
//   - AsyncClientClass + FirebaseApp init/loop (token + RTDB auth)
//   - RealtimeDatabase::set / update / get with the path scheme in
//     docs/api.md (radiant/telemetry/..., radiant/config/...)

FirebaseSync::FirebaseSync(const char* url, const char* secret)
  : _url(url), _secret(secret) {}

void FirebaseSync::begin() {
  // TODO: connect Wi-Fi, configure async FirebaseClient, register auth
  //       (prefer Firebase Auth token; legacy database secret as fallback)
}

void FirebaseSync::loop() {
  // TODO: call the library's async loop so requests keep progressing
}

bool FirebaseSync::setJson(const char* path, const char* json) {
  // TODO: RealtimeDatabase set -> returns true when the write completes
  (void)path;
  (void)json;
  return false;
}

bool FirebaseSync::getString(const char* path, String& out) {
  // TODO: RealtimeDatabase get -> fills `out` with the node JSON
  (void)path;
  (void)out;
  return false;
}

bool FirebaseSync::connected() const {
  // TODO: report Wi-Fi + Firebase session state
  return false;
}
