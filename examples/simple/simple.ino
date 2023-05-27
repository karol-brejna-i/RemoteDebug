/**
 * Basic example of RemoteDebug library usage.
 *
 * This example connects to WiFi (remember to set up ssid and password in the code), and
 * initialize the RemoteDebug library.
 *
 * After that, the following logic is executed:
 * - Each second, the led is blinked and a message is sent to RemoteDebug (in verbose level)
 * - Each 5 seconds, a message is sent to RemoteDebug in all levels (verbose, debug, info, warning and error) and a function is called
 *
 * Before running, decide if you want to use mDNS (change the define USE_MDNS to true or false).
 */

#define HOST_NAME "remotedebug"

#if defined ESP8266 || defined ESP32
#define USE_MDNS true
#else
#error "The board must be ESP8266 or ESP32"
#endif  // ESP

#if defined ESP8266
#include <ESP8266WiFi.h>
#ifdef USE_MDNS
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#endif  // ESP8266

#elif defined ESP32
#include <WiFi.h>
#ifdef USE_MDNS
#include <DNSServer.h>

#include "ESPmDNS.h"
#endif
#endif  // ESP32

#include "RemoteDebug.h"
RemoteDebug Debug;

// SSID and password
const char* ssid = "........";
const char* password = "........";

// Time
uint32_t mLastTime = 0;
uint32_t mTimeSeconds = 0;

// Function example to show a new auto function name of debug* macros
void foo() {
    uint8_t var = 1;

    debugV("this is a debug - var %u", var);
    debugV("This is a println");
}

void setup() {
    // Initialize the Serial (use only in setup codes)
    Serial.begin(115200);

    // Builtin led of ESP
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // Initialize WiFi
    Serial.println("**** Setup: initializing ...");
    WiFi.begin(ssid, password);
    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.print("Connected to IP address: ");
    Serial.println(WiFi.localIP());

    // Register host name in WiFi and mDNS
    String hostNameWifi = HOST_NAME;
    hostNameWifi.concat(".local");

#ifdef ESP8266  // Only for ESP8266
    WiFi.hostname(hostNameWifi);
#endif

#ifdef USE_MDNS  // Use the MDNS ?
    if (MDNS.begin(HOST_NAME)) {
        Serial.print("* MDNS responder started. Hostname -> ");
        Serial.println(HOST_NAME);
    }

    MDNS.addService("telnet", "tcp", 23);
#endif

    // Initialize RemoteDebug
    Debug.begin(HOST_NAME);          // Initialize the WiFi server
    Debug.setResetCmdEnabled(true);  // Enable the reset command
    Debug.showProfiler(true);        // Profiler (Good to measure times, to optimize codes)
    Debug.showColors(true);          // Colors

    Serial.println("* Arduino RemoteDebug Library example");
    Serial.println("*");
    Serial.printf("* WiFI connected. IP address: %s\n", WiFi.localIP().toString().c_str());
    Serial.println("*");
    Serial.println("* Please use the telnet client (telnet for Mac/Unix or putty and others for Windows)");
    Serial.println("* or the RemoteDebugApp (in browser: http://joaolopesf.net/remotedebugapp)");
    Serial.println("*");
    Serial.println("* This sample will send messages of debug in all levels.");
    Serial.println("*");
    Serial.println("* Please try change debug level in client (telnet or web app), to see how it works");
    Serial.println("*");
}

void loop() {
    // Each second
    if ((millis() - mLastTime) >= 1000) {
        // Time
        mLastTime = millis();
        mTimeSeconds++;

        // Blink the led
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

        // Debug the time (verbose level)
        debugV("* Time: %u seconds (VERBOSE)", mTimeSeconds);

        if (mTimeSeconds % 5 == 0) {  // Each 5 seconds
            // Debug levels
            debugV("* This is a message of debug level VERBOSE");
            debugD("* This is a message of debug level DEBUG");
            debugI("* This is a message of debug level INFO");
            debugW("* This is a message of debug level WARNING");
            debugE("* This is a message of debug level ERROR");

            // Call a function
            foo();
        }
    }

    // RemoteDebug handle
    Debug.handle();

    // Give a time for ESP
    yield();
}
