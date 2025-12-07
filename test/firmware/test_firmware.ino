/**
 * RemoteDebug Test Firmware
 * 
 * This firmware provides a comprehensive test environment for automated
 * integration testing of the RemoteDebug library.
 * 
 * Features:
 * - All standard RemoteDebug functionality
 * - Custom test commands for triggering specific behaviors
 * - Periodic heartbeat messages for connection verification
 * - Predictable, testable output patterns
 * 
 * Test Commands:
 *   test_all_levels  - Output one message at each debug level
 *   test_flood       - Send 100 messages quickly (rate limiting test)
 *   test_long        - Send a 500-character message (buffer test)
 *   test_special     - Send message with special characters
 *   test_status      - Output current test state
 *   test_colors      - Output colored messages for each level
 * 
 * Usage:
 *   1. Update WIFI_SSID and WIFI_PASSWORD below
 *   2. Flash to ESP32 or ESP8266
 *   3. Connect via telnet: telnet <device_ip> 23
 *   4. Run test commands or use test/integration/run_tests.sh
 */

//------------------------------------------------------------------------------
// Configuration
//------------------------------------------------------------------------------

// WiFi credentials - UPDATE THESE
#define WIFI_SSID     "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"

// Device configuration
#define HOSTNAME "remotedebug-test"
#define TELNET_PORT 23

// Test configuration
#define HEARTBEAT_INTERVAL_MS 5000      // Send heartbeat every 5 seconds
#define TEST_COUNTER_MAX      1000000   // Reset counter after this value

// Uncomment to enable password protection testing
// #define TEST_PASSWORD "test123"

//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------

#if defined(ESP8266)
    #include <ESP8266WiFi.h>
    #include <ESP8266mDNS.h>
#elif defined(ESP32)
    #include <WiFi.h>
    #include <ESPmDNS.h>
#else
    #error "This firmware requires ESP8266 or ESP32"
#endif

#include "RemoteDebug.h"

//------------------------------------------------------------------------------
// Global Variables
//------------------------------------------------------------------------------

RemoteDebug Debug;

// Test state
uint32_t testCounter = 0;
uint32_t lastHeartbeat = 0;
uint32_t bootTime = 0;

//------------------------------------------------------------------------------
// Test Commands Implementation
//------------------------------------------------------------------------------

/**
 * Output one message at each debug level.
 * Useful for testing level filtering.
 */
void cmdTestAllLevels() {
    testCounter++;
    debugV("TEST_VERBOSE_%lu", testCounter);
    debugD("TEST_DEBUG_%lu", testCounter);
    debugI("TEST_INFO_%lu", testCounter);
    debugW("TEST_WARNING_%lu", testCounter);
    debugE("TEST_ERROR_%lu", testCounter);
}

/**
 * Send many messages quickly.
 * Useful for testing rate limiting and buffer handling.
 */
void cmdTestFlood() {
    debugI("TEST_FLOOD_START");
    for (int i = 0; i < 100; i++) {
        debugI("FLOOD_%03d", i);
    }
    debugI("TEST_FLOOD_END");
}

/**
 * Send a very long message.
 * Useful for testing message buffer limits.
 */
void cmdTestLongMessage() {
    // Create a 500-character message
    String longMsg = "LONG_MESSAGE_START:";
    for (int i = 0; i < 48; i++) {
        longMsg += "0123456789";  // Add 10 chars each iteration
    }
    longMsg += ":END";
    
    debugI("%s", longMsg.c_str());
    debugI("Long message length: %d", longMsg.length());
}

/**
 * Send message with special characters.
 * Useful for testing character handling.
 */
void cmdTestSpecialChars() {
    debugI("Special chars test:");
    debugI("  Tab character: [\t] done");
    debugI("  Quotes: \"double\" and 'single'");
    debugI("  Backslash: \\ path\\to\\file");
    debugI("  Percent: 100%% complete");
    debugI("  Numbers: 0123456789");
    debugI("  Symbols: !@#$^&*()_+-=[]{}|;:,.<>?");
}

/**
 * Output current test state.
 * Useful for verification and debugging.
 */
void cmdTestStatus() {
    uint32_t uptime = (millis() - bootTime) / 1000;
    uint32_t freeHeap = ESP.getFreeHeap();
    
    debugI("=== Test Status ===");
    debugI("Counter: %lu", testCounter);
    debugI("Free Heap: %lu bytes", freeHeap);
    debugI("Uptime: %lu seconds", uptime);
    debugI("WiFi RSSI: %d dBm", WiFi.RSSI());
    debugI("IP: %s", WiFi.localIP().toString().c_str());
    debugI("Connected: %s", Debug.isConnected() ? "yes" : "no");
    debugI("==================");
}

/**
 * Output colored messages for visual testing.
 */
void cmdTestColors() {
    debugI("Color test - each level should have different color:");
    debugV("VERBOSE - typically green");
    debugD("DEBUG - typically light green");
    debugI("INFO - typically yellow");
    debugW("WARNING - typically cyan");
    debugE("ERROR - typically red");
}

/**
 * Echo back the argument.
 * Useful for round-trip testing.
 */
void cmdTestEcho(const String& args) {
    debugI("ECHO: %s", args.c_str());
}

//------------------------------------------------------------------------------
// Command Processing
//------------------------------------------------------------------------------

void processTestCommands() {
    String cmd = Debug.getLastCommand();
    
    if (cmd.isEmpty()) {
        return;
    }
    
    // Extract command and arguments
    String command = cmd;
    String args = "";
    int spacePos = cmd.indexOf(' ');
    if (spacePos > 0) {
        command = cmd.substring(0, spacePos);
        args = cmd.substring(spacePos + 1);
    }
    
    // Process test commands
    if (command == "test_all_levels") {
        cmdTestAllLevels();
        Debug.clearLastCommand();
    }
    else if (command == "test_flood") {
        cmdTestFlood();
        Debug.clearLastCommand();
    }
    else if (command == "test_long") {
        cmdTestLongMessage();
        Debug.clearLastCommand();
    }
    else if (command == "test_special") {
        cmdTestSpecialChars();
        Debug.clearLastCommand();
    }
    else if (command == "test_status") {
        cmdTestStatus();
        Debug.clearLastCommand();
    }
    else if (command == "test_colors") {
        cmdTestColors();
        Debug.clearLastCommand();
    }
    else if (command == "test_echo") {
        cmdTestEcho(args);
        Debug.clearLastCommand();
    }
    else if (command == "test_help") {
        debugI("=== Test Commands ===");
        debugI("test_all_levels - Output at each debug level");
        debugI("test_flood      - Send 100 messages quickly");
        debugI("test_long       - Send 500-char message");
        debugI("test_special    - Send special characters");
        debugI("test_status     - Show device status");
        debugI("test_colors     - Show colored messages");
        debugI("test_echo <msg> - Echo back the message");
        debugI("test_help       - Show this help");
        debugI("=====================");
        Debug.clearLastCommand();
    }
    // Note: Standard commands (h, v, d, i, w, e, m, etc.) are handled by RemoteDebug
}

//------------------------------------------------------------------------------
// Periodic Tasks
//------------------------------------------------------------------------------

void sendHeartbeat() {
    if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        testCounter++;
        if (testCounter > TEST_COUNTER_MAX) {
            testCounter = 0;
        }
        
        debugV("HEARTBEAT_%lu uptime=%lus heap=%lu", 
               testCounter, 
               (millis() - bootTime) / 1000,
               ESP.getFreeHeap());
        
        lastHeartbeat = millis();
    }
}

//------------------------------------------------------------------------------
// WiFi Setup
//------------------------------------------------------------------------------

bool setupWiFi() {
    Serial.println("\n[WiFi] Connecting to " WIFI_SSID "...");
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 60) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[WiFi] Connection failed!");
        return false;
    }
    
    Serial.println();
    Serial.println("[WiFi] Connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WiFi] RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    return true;
}

bool setupMDNS() {
    if (MDNS.begin(HOSTNAME)) {
        Serial.print("[mDNS] Hostname: ");
        Serial.print(HOSTNAME);
        Serial.println(".local");
        MDNS.addService("telnet", "tcp", TELNET_PORT);
        return true;
    }
    Serial.println("[mDNS] Setup failed");
    return false;
}

//------------------------------------------------------------------------------
// Setup
//------------------------------------------------------------------------------

void setup() {
    bootTime = millis();
    
    // Initialize Serial
    Serial.begin(115200);
    delay(100);
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("  RemoteDebug Test Firmware v1.0");
    Serial.println("========================================");
    
    // Connect to WiFi
    if (!setupWiFi()) {
        Serial.println("[ERROR] Cannot continue without WiFi");
        while (true) {
            delay(1000);
        }
    }
    
    // Setup mDNS
    setupMDNS();
    
    // Initialize RemoteDebug
    Debug.begin(HOSTNAME, TELNET_PORT);
    Debug.setResetCmdEnabled(true);
    Debug.showColors(true);
    Debug.showTime(true);
    Debug.showProfiler(false);
    Debug.setSerialEnabled(true);  // Also output to Serial
    
    #ifdef TEST_PASSWORD
    Debug.setPassword(TEST_PASSWORD);
    Serial.print("[Security] Password protection enabled: ");
    Serial.println(TEST_PASSWORD);
    #endif
    
    Serial.println("[RemoteDebug] Initialized");
    Serial.print("[RemoteDebug] Connect: telnet ");
    Serial.print(WiFi.localIP());
    Serial.print(" ");
    Serial.println(TELNET_PORT);
    Serial.println();
    
    // Startup message
    debugI("TEST_FIRMWARE_READY v1.0");
    debugI("Hostname: %s", HOSTNAME);
    debugI("IP: %s", WiFi.localIP().toString().c_str());
    debugI("Type 'test_help' for test commands");
}

//------------------------------------------------------------------------------
// Main Loop
//------------------------------------------------------------------------------

void loop() {
    // Handle RemoteDebug
    Debug.handle();
    
    // Process custom test commands
    processTestCommands();
    
    // Send periodic heartbeat
    sendHeartbeat();
    
    // Small delay to prevent watchdog issues
    delay(1);
}
