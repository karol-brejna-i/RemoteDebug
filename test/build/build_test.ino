/**
 * Minimal build test - verifies the library compiles.
 * This is NOT a working example - see examples/ folder for usage.
 */

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#else
#error "Requires ESP8266 or ESP32"
#endif

#include "RemoteDebug.h"

RemoteDebug Debug;

void setup() {
    Debug.begin("build_test");
}

void loop() {
    Debug.handle();
}
