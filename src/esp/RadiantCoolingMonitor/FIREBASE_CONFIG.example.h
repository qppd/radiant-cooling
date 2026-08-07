/*
 * FIREBASE_CONFIG.example.h - template for Firebase Realtime Database credentials
 *
 * 1. Copy this file to FIREBASE_CONFIG.h   (same folder)
 * 2. Fill in your database URL, Web API key, and auth credentials
 * 3. Upload the sketch - FIREBASE_CONFIG.h is git-ignored, so the real
 *    credentials stay local and are never committed.
 *
 * Setup: firebase.google.com -> project -> Realtime Database (get the URL)
 * and Project settings -> General (Web API key). Enable Anonymous or
 * Email/Password sign-in under Authentication -> Sign-in method.
 */
#pragma once
#include <Arduino.h>

// ---- Firebase Realtime Database (Mobizt FirebaseClient) ----
static const char FIREBASE_URL[]      = "https://<project>-default-rtdb.firebaseio.com/";
static const char FIREBASE_API_KEY[]  = "your-web-api-key";   // Firebase Web API key
static const char FIREBASE_EMAIL[]    = "";                   // "" = anonymous sign-in
static const char FIREBASE_PASSWORD[] = "";                   // email/password auth
