/*
 * CustomFirebaseFS.h - build options override for the Firebase ESP Client
 * library (RadiantCoolingMonitor only).
 *
 * The library's FirebaseFS.h enables ALL features by default (RTDB, FCM,
 * Firestore, Storage, OTA, PSRAM, SD, Flash FS, error queue...), which blows
 * past the ESP32's 1.31 MB app partition. This sketch only uses RTDB
 * streaming + writes, so we disable every other feature.
 *
 * This file is picked up automatically via __has_include("CustomFirebaseFS.h")
 * in FirebaseFS.h. Keep it in the sketch folder.
 */
#ifndef CUSTOM_FIREBASE_FS_H
#define CUSTOM_FIREBASE_FS_H

#define DISABLE_FCM                // No cloud messaging
#define DISABLE_ERROR_QUEUE        // No offline write queue
#define DISABLE_OTA                // No OTA firmware update support
#define DISABLE_PSRAM              // No PSRAM on this board
#define DISABLE_SD                 // No SD card storage
#define DISABLE_FLASH              // No LittleFS/SPIFFS storage

#endif // CUSTOM_FIREBASE_FS_H