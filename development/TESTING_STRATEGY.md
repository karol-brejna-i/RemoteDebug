# RemoteDebug Testing Strategy

This document outlines a comprehensive testing strategy for the RemoteDebug library, covering unit tests, integration tests, and end-to-end tests.

---

## Table of Contents

1. [Testing Challenges](#1-testing-challenges)
2. [Testing Approach Overview](#2-testing-approach-overview)
3. [Unit Testing Strategy](#3-unit-testing-strategy)
4. [Integration Testing Strategy](#4-integration-testing-strategy)
5. [Test Firmware for Hardware Testing](#5-test-firmware-for-hardware-testing)
6. [Priority Test Cases](#6-priority-test-cases)
7. [CI/CD Integration](#7-cicd-integration)
8. [Implementation Roadmap](#8-implementation-roadmap)

---

## 1. Testing Challenges

### Current Code Limitations

The RemoteDebug library presents several testing challenges:

| Challenge | Description | Impact |
|-----------|-------------|--------|
| **Tight Hardware Coupling** | Direct use of `WiFiServer`, `WiFiClient`, ESP-specific APIs | Hard to mock for unit tests |
| **Global State** | Static variables (`_instance`, `TelnetServer`, `TelnetClient`) | Tests can interfere with each other |
| **Macro-Heavy Interface** | Debug macros (`debugV`, `rdebugI`, etc.) | Macros can't be easily tested in isolation |
| **Conditional Compilation** | `#ifdef` blocks for features | Need to test multiple build configurations |
| **Side Effects** | Commands like `reset`, `cpu80/160` affect hardware | Can't safely test on host |

### What We CAN Test

Despite these challenges, significant portions are testable:

- **Command parsing logic** - String processing, command recognition
- **Message formatting** - Color codes, level prefixes, timestamps
- **Protocol handling** - Telnet command sequences, CR/LF handling
- **State machine logic** - Connection states, password flow
- **Configuration validation** - Port numbers, timeouts, log levels

---

## 2. Testing Approach Overview

We'll use a **three-tier testing strategy**:

```
┌─────────────────────────────────────────────────────────┐
│  Tier 3: End-to-End Tests (Hardware)                   │
│  • Real ESP32 device with test firmware                │
│  • Automated telnet/nc scripts                         │
│  • Manual testing for edge cases                       │
├─────────────────────────────────────────────────────────┤
│  Tier 2: Integration Tests (Wokwi Simulator)           │
│  • Simulated ESP32 running real firmware               │
│  • Network communication tests                         │
│  • Runs in CI/CD                                       │
├─────────────────────────────────────────────────────────┤
│  Tier 1: Unit Tests (Native Host)                      │
│  • Pure logic testing with mocks                       │
│  • Fast, runs on every commit                          │
│  • Command parsing, formatting, state machines         │
└─────────────────────────────────────────────────────────┘
```

---

## 3. Unit Testing Strategy

### 3.1 Framework Choice: PlatformIO Native + Unity

Use PlatformIO's native testing with the Unity framework:

```ini
; platformio.ini additions for unit testing
[env:native]
platform = native
test_framework = unity
build_flags = 
    -D UNIT_TEST
    -D DEBUG_DISABLED=0
test_build_src = false
```

### 3.2 Testable Components (Extract and Test)

To make the code testable, we should extract pure logic into separate classes/functions:

#### A. Command Parser (New Component)

```cpp
// src/CommandParser.h (proposed)
class CommandParser {
public:
    struct ParsedCommand {
        String command;
        String options;
        bool valid;
    };
    
    static ParsedCommand parse(const String& input);
    static bool isBuiltInCommand(const String& cmd);
    static uint8_t getDebugLevelForCommand(const String& cmd);
};
```

**Test Cases:**
```cpp
void test_parse_simple_command() {
    auto result = CommandParser::parse("v");
    TEST_ASSERT_EQUAL_STRING("v", result.command.c_str());
    TEST_ASSERT_TRUE(result.options.isEmpty());
}

void test_parse_command_with_options() {
    auto result = CommandParser::parse("filter message");
    TEST_ASSERT_EQUAL_STRING("filter", result.command.c_str());
    TEST_ASSERT_EQUAL_STRING("message", result.options.c_str());
}

void test_debug_level_commands() {
    TEST_ASSERT_EQUAL(VERBOSE, CommandParser::getDebugLevelForCommand("v"));
    TEST_ASSERT_EQUAL(DEBUG, CommandParser::getDebugLevelForCommand("d"));
    TEST_ASSERT_EQUAL(INFO, CommandParser::getDebugLevelForCommand("i"));
    TEST_ASSERT_EQUAL(WARNING, CommandParser::getDebugLevelForCommand("w"));
    TEST_ASSERT_EQUAL(ERROR, CommandParser::getDebugLevelForCommand("e"));
}
```

#### B. Message Formatter (New Component)

```cpp
// src/MessageFormatter.h (proposed)
class MessageFormatter {
public:
    static String formatWithLevel(const String& message, uint8_t level, bool colors);
    static String formatWithTimestamp(const String& message, uint32_t millis);
    static String getColorForLevel(uint8_t level);
    static String getLevelPrefix(uint8_t level);
};
```

**Test Cases:**
```cpp
void test_level_prefix() {
    TEST_ASSERT_EQUAL_STRING("(V) ", MessageFormatter::getLevelPrefix(VERBOSE));
    TEST_ASSERT_EQUAL_STRING("(D) ", MessageFormatter::getLevelPrefix(DEBUG));
    TEST_ASSERT_EQUAL_STRING("(E) ", MessageFormatter::getLevelPrefix(ERROR));
}

void test_color_codes() {
    TEST_ASSERT_EQUAL_STRING("\x1B[0;32m", MessageFormatter::getColorForLevel(VERBOSE));
    TEST_ASSERT_EQUAL_STRING("\x1B[1;31m", MessageFormatter::getColorForLevel(ERROR));
}

void test_format_with_colors() {
    String result = MessageFormatter::formatWithLevel("test", VERBOSE, true);
    TEST_ASSERT_TRUE(result.startsWith("\x1B[0;32m"));
    TEST_ASSERT_TRUE(result.indexOf("test") > 0);
}
```

#### C. Input Buffer Handler (New Component)

```cpp
// src/InputBuffer.h (proposed)
class InputBuffer {
public:
    void addChar(char c);
    bool hasCompleteCommand() const;
    String getCommand();
    void clear();
    
private:
    String _buffer;
    bool _complete = false;
};
```

**Test Cases:**
```cpp
void test_simple_command_input() {
    InputBuffer buf;
    buf.addChar('h');
    buf.addChar('e');
    buf.addChar('l');
    buf.addChar('p');
    buf.addChar('\n');
    
    TEST_ASSERT_TRUE(buf.hasCompleteCommand());
    TEST_ASSERT_EQUAL_STRING("help", buf.getCommand().c_str());
}

void test_crlf_handling() {
    InputBuffer buf;
    buf.addChar('h');
    buf.addChar('\r');
    buf.addChar('\n');
    
    TEST_ASSERT_TRUE(buf.hasCompleteCommand());
    TEST_ASSERT_EQUAL_STRING("h", buf.getCommand().c_str());
}

void test_ignores_non_printable() {
    InputBuffer buf;
    buf.addChar('t');
    buf.addChar('\x00');  // NULL
    buf.addChar('e');
    buf.addChar('\x07');  // BELL
    buf.addChar('s');
    buf.addChar('t');
    buf.addChar('\n');
    
    TEST_ASSERT_EQUAL_STRING("test", buf.getCommand().c_str());
}
```

### 3.3 Mock Objects

For testing components that need WiFi:

```cpp
// test/mocks/MockWiFiClient.h
class MockWiFiClient {
public:
    void setInputData(const String& data) { _input = data; _pos = 0; }
    bool connected() { return _connected; }
    int available() { return _input.length() - _pos; }
    char read() { return _pos < _input.length() ? _input[_pos++] : -1; }
    size_t println(const String& s) { _output += s + "\n"; return s.length() + 1; }
    String getOutput() { return _output; }
    
    bool _connected = true;
    String _input;
    String _output;
    size_t _pos = 0;
};
```

---

## 4. Integration Testing Strategy

### 4.1 Wokwi Simulator Integration

[Wokwi](https://wokwi.com) can simulate ESP32 with network support. This allows running real firmware in CI.

#### Wokwi Configuration

```json
// wokwi.toml
[wokwi]
version = 1
firmware = ".pio/build/esp32/firmware.bin"
elf = ".pio/build/esp32/firmware.elf"

[[net.forward]]
from = "localhost:2323"
to = "target:23"
```

```json
// diagram.json
{
  "version": 1,
  "author": "RemoteDebug",
  "editor": "wokwi",
  "parts": [
    { "type": "wokwi-esp32-devkit-v1", "id": "esp" }
  ],
  "connections": []
}
```

### 4.2 Integration Test Script

```bash
#!/bin/bash
# test/integration/run_telnet_tests.sh

DEVICE_IP="${DEVICE_IP:-localhost}"
DEVICE_PORT="${DEVICE_PORT:-23}"
TIMEOUT=5

# Helper function to send command and capture response
send_command() {
    local cmd="$1"
    local expected="$2"
    
    response=$(echo -e "$cmd" | timeout $TIMEOUT nc -q 1 $DEVICE_IP $DEVICE_PORT 2>/dev/null)
    
    if echo "$response" | grep -q "$expected"; then
        echo "✅ PASS: '$cmd' -> found '$expected'"
        return 0
    else
        echo "❌ FAIL: '$cmd' -> expected '$expected', got: $response"
        return 1
    fi
}

# Wait for device to be ready
wait_for_device() {
    echo "Waiting for device at $DEVICE_IP:$DEVICE_PORT..."
    for i in {1..30}; do
        if nc -z $DEVICE_IP $DEVICE_PORT 2>/dev/null; then
            echo "Device ready!"
            return 0
        fi
        sleep 1
    done
    echo "Device not available"
    return 1
}

# Test suite
run_tests() {
    local failed=0
    
    echo "=== RemoteDebug Integration Tests ==="
    echo ""
    
    # Test 1: Help command
    send_command "h" "Commands:" || ((failed++))
    
    # Test 2: Debug level change
    send_command "v" "Verbose" || ((failed++))
    send_command "d" "Debug" || ((failed++))
    send_command "i" "Info" || ((failed++))
    send_command "w" "Warning" || ((failed++))
    send_command "e" "Error" || ((failed++))
    
    # Test 3: Memory command
    send_command "m" "Free Heap" || ((failed++))
    
    # Test 4: Show colors
    send_command "c" "color" || ((failed++))
    
    echo ""
    echo "=== Results: $((8 - failed))/8 passed ==="
    
    return $failed
}

# Main
wait_for_device && run_tests
exit $?
```

---

## 5. Test Firmware for Hardware Testing

### 5.1 Dedicated Test Firmware

Create a special firmware build for comprehensive testing:

```cpp
// test/firmware/test_firmware.ino
/**
 * RemoteDebug Test Firmware
 * 
 * This firmware provides a comprehensive test environment for
 * integration testing of the RemoteDebug library.
 * 
 * Features:
 * - Periodic messages at all log levels
 * - Custom commands for triggering specific behaviors
 * - Memory and timing information
 * - Predictable, testable output patterns
 */

#include <WiFi.h>
#include "RemoteDebug.h"

RemoteDebug Debug;

// Test configuration
const char* TEST_SSID = "TestNetwork";
const char* TEST_PASSWORD = "testpass123";
const char* HOSTNAME = "remotedebug-test";

// Test state
uint32_t testCounter = 0;
uint32_t lastTestOutput = 0;

// Custom test commands
void processTestCommands() {
    String cmd = Debug.getLastCommand();
    
    if (cmd == "test_all_levels") {
        // Output one message at each level for testing
        debugV("TEST_VERBOSE_%lu", testCounter);
        debugD("TEST_DEBUG_%lu", testCounter);
        debugI("TEST_INFO_%lu", testCounter);
        debugW("TEST_WARNING_%lu", testCounter);
        debugE("TEST_ERROR_%lu", testCounter);
        testCounter++;
        Debug.clearLastCommand();
    }
    else if (cmd == "test_flood") {
        // Send many messages quickly for rate limiting tests
        for (int i = 0; i < 100; i++) {
            debugI("FLOOD_%d", i);
        }
        Debug.clearLastCommand();
    }
    else if (cmd == "test_long_message") {
        // Send a long message for buffer tests
        String longMsg = "";
        for (int i = 0; i < 50; i++) {
            longMsg += "ABCDEFGHIJ";
        }
        debugI("%s", longMsg.c_str());
        Debug.clearLastCommand();
    }
    else if (cmd == "test_special_chars") {
        // Test special characters
        debugI("Special: \t tab, quotes: \"test\", backslash: \\");
        Debug.clearLastCommand();
    }
    else if (cmd == "test_status") {
        // Output current test state
        debugI("TEST_STATUS: counter=%lu, heap=%lu, uptime=%lu", 
               testCounter, ESP.getFreeHeap(), millis());
        Debug.clearLastCommand();
    }
    else if (cmd == "test_echo") {
        // Echo the next input (for round-trip testing)
        debugI("ECHO_READY");
        Debug.clearLastCommand();
    }
}

void setup() {
    Serial.begin(115200);
    
    WiFi.begin(TEST_SSID, TEST_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());
    
    Debug.begin(HOSTNAME);
    Debug.setResetCmdEnabled(true);
    Debug.showColors(true);
    Debug.showTime(true);
    
    debugI("TEST_FIRMWARE_READY v1.0");
}

void loop() {
    Debug.handle();
    
    // Custom test command processing
    processTestCommands();
    
    // Periodic test output (every 5 seconds)
    if (millis() - lastTestOutput > 5000) {
        debugV("HEARTBEAT_%lu", millis() / 1000);
        lastTestOutput = millis();
    }
}
```

### 5.2 Test Scenarios with Expected Outputs

| Test Scenario | Command/Action | Expected Response |
|---------------|----------------|-------------------|
| **Connection Welcome** | Connect via telnet | Welcome message with hostname |
| **Help Display** | `h` or `?` | List of available commands |
| **Level Verbose** | `v` | "Debug level set to Verbose" |
| **Level Debug** | `d` | "Debug level set to Debug" |
| **Level Info** | `i` | "Debug level set to Info" |
| **Level Warning** | `w` | "Debug level set to Warning" |
| **Level Error** | `e` | "Debug level set to Error" |
| **Memory Info** | `m` | "Free Heap RAM: [number]" |
| **Colors Toggle** | `c` | Color/no-color toggle confirmation |
| **Silence Mode** | `s` | Enters silence mode |
| **Filter Set** | `f test` | "Filter: test" |
| **Filter Clear** | `f` | Filter cleared |
| **Time Toggle** | `t` | Time display toggled |
| **Profiler Toggle** | `p` | Profiler toggled |
| **Quit** | `q` | Connection closed |
| **All Levels Output** | `test_all_levels` | Messages at V, D, I, W, E levels |
| **Long Message** | `test_long_message` | 500-char message received intact |
| **Special Chars** | `test_special_chars` | Correct handling of tabs, quotes |

### 5.3 Password Protection Tests

```cpp
// Test firmware with password enabled
Debug.begin(HOSTNAME);
Debug.setPassword("secret123");

// Test scenarios:
// 1. Connect -> Should prompt for password
// 2. Wrong password -> "Wrong password!" + disconnect after N attempts
// 3. Correct password -> "Password ok" + help shown
// 4. Commands before auth -> Should be rejected
```

---

## 6. Priority Test Cases

### Critical Tests (Must Have for Improvements)

Based on the planned improvements, these tests are **highest priority**:

#### 6.1 Command Processing Tests

These are critical for **improvement 2.3 (Event-Driven Architecture for Commands)**:

```cpp
// Test custom command registration
void test_custom_command_registration() {
    bool commandCalled = false;
    Debug.registerCommand("mytest", [&](const String& args) {
        commandCalled = true;
    });
    
    simulateInput("mytest\n");
    TEST_ASSERT_TRUE(commandCalled);
}

// Test command with arguments
void test_command_with_arguments() {
    String receivedArgs;
    Debug.registerCommand("setval", [&](const String& args) {
        receivedArgs = args;
    });
    
    simulateInput("setval 42\n");
    TEST_ASSERT_EQUAL_STRING("42", receivedArgs.c_str());
}
```

#### 6.2 Log Level Filtering Tests

Critical for **improvement 3.2 (Compile-Time Log Levels)**:

```cpp
void test_level_filtering_verbose() {
    Debug.setDebugLevel(VERBOSE);
    TEST_ASSERT_TRUE(Debug.isActive(VERBOSE));
    TEST_ASSERT_TRUE(Debug.isActive(DEBUG));
    TEST_ASSERT_TRUE(Debug.isActive(INFO));
    TEST_ASSERT_TRUE(Debug.isActive(WARNING));
    TEST_ASSERT_TRUE(Debug.isActive(ERROR));
}

void test_level_filtering_error_only() {
    Debug.setDebugLevel(ERROR);
    TEST_ASSERT_FALSE(Debug.isActive(VERBOSE));
    TEST_ASSERT_FALSE(Debug.isActive(DEBUG));
    TEST_ASSERT_FALSE(Debug.isActive(INFO));
    TEST_ASSERT_FALSE(Debug.isActive(WARNING));
    TEST_ASSERT_TRUE(Debug.isActive(ERROR));
}
```

#### 6.3 Connection Handler Tests

Critical for **improvement 2.1 (Separate Connection Handlers)**:

```cpp
// Test Telnet connection
void test_telnet_connect_disconnect() {
    // Connect
    nc_connect();
    TEST_ASSERT_TRUE(Debug.isConnected());
    
    // Disconnect
    nc_disconnect();
    delay(100);
    TEST_ASSERT_FALSE(Debug.isConnected());
}

// Test inactivity timeout
void test_inactivity_disconnect() {
    nc_connect();
    TEST_ASSERT_TRUE(Debug.isConnected());
    
    // Wait for timeout
    delay(MAX_TIME_INACTIVE + 1000);
    Debug.handle();
    
    TEST_ASSERT_FALSE(Debug.isConnected());
}
```

#### 6.4 Port Configuration Tests

Critical for **improvement 3.7 (Configurable Port Numbers)**:

```cpp
void test_custom_port() {
    Debug.begin("test", 2323);  // Custom port
    
    // Should connect on port 2323
    bool connected = nc_connect_port(2323);
    TEST_ASSERT_TRUE(connected);
    TEST_ASSERT_TRUE(Debug.isConnected());
}

void test_default_port() {
    Debug.begin("test");  // Default port 23
    
    bool connected = nc_connect_port(23);
    TEST_ASSERT_TRUE(connected);
}
```

#### 6.5 Password Authentication Tests

Critical for **improvement 6.1 (Improve Password Security)**:

```cpp
void test_password_required() {
    Debug.setPassword("secret");
    nc_connect();
    
    String response = nc_read();
    TEST_ASSERT_TRUE(response.indexOf("password") >= 0);
}

void test_password_correct() {
    Debug.setPassword("secret");
    nc_connect();
    nc_send("secret\n");
    
    String response = nc_read();
    TEST_ASSERT_TRUE(response.indexOf("Password ok") >= 0);
}

void test_password_wrong_lockout() {
    Debug.setPassword("secret");
    nc_connect();
    
    // Try wrong passwords
    for (int i = 0; i < REMOTEDEBUG_PWD_ATTEMPTS + 1; i++) {
        nc_send("wrong\n");
    }
    
    // Should be disconnected
    TEST_ASSERT_FALSE(nc_is_connected());
}
```

#### 6.6 Ring Buffer Tests (Future)

For **improvement 3.3 (Log Ring Buffer)**:

```cpp
void test_ring_buffer_captures_before_connect() {
    Debug.setRingBufferSize(10);
    
    // Log messages before connection
    debugI("Message 1");
    debugI("Message 2");
    debugI("Message 3");
    
    // Connect
    nc_connect();
    
    // Should receive buffered messages
    String response = nc_read_all();
    TEST_ASSERT_TRUE(response.indexOf("Message 1") >= 0);
    TEST_ASSERT_TRUE(response.indexOf("Message 2") >= 0);
    TEST_ASSERT_TRUE(response.indexOf("Message 3") >= 0);
}

void test_ring_buffer_overflow() {
    Debug.setRingBufferSize(3);
    
    debugI("Msg 1");  // Will be evicted
    debugI("Msg 2");
    debugI("Msg 3");
    debugI("Msg 4");  // Newest
    
    nc_connect();
    String response = nc_read_all();
    
    TEST_ASSERT_TRUE(response.indexOf("Msg 1") < 0);  // Evicted
    TEST_ASSERT_TRUE(response.indexOf("Msg 4") >= 0); // Present
}
```

---

## 7. CI/CD Integration

### 7.1 GitHub Actions Workflow for Tests

```yaml
# .github/workflows/tests.yml
name: Tests

on:
  push:
    branches: [master, main, develop]
  pull_request:
    branches: [master, main, develop]

jobs:
  # Unit tests on native platform
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'
      
      - name: Install PlatformIO
        run: pip install platformio
      
      - name: Run Unit Tests
        run: pio test -e native --verbose

  # Integration tests with Wokwi
  integration-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'
      
      - name: Install PlatformIO
        run: pip install platformio
      
      - name: Build Test Firmware
        run: pio run -e esp32-test
      
      - name: Install Wokwi CLI
        run: |
          curl -L https://wokwi.com/ci/install.sh | sh
          echo "$HOME/.wokwi/bin" >> $GITHUB_PATH
      
      - name: Run Wokwi Simulation
        env:
          WOKWI_CLI_TOKEN: ${{ secrets.WOKWI_CLI_TOKEN }}
        run: |
          wokwi-cli --timeout 60000 &
          WOKWI_PID=$!
          sleep 10  # Wait for simulator
          
          # Run integration tests
          ./test/integration/run_telnet_tests.sh
          TEST_RESULT=$?
          
          kill $WOKWI_PID
          exit $TEST_RESULT
```

### 7.2 Hardware-in-the-Loop (Optional)

For organizations with physical test hardware:

```yaml
# Self-hosted runner with ESP32 connected
hardware-tests:
  runs-on: [self-hosted, esp32]
  steps:
    - uses: actions/checkout@v4
    
    - name: Flash Test Firmware
      run: pio run -e esp32-test -t upload
    
    - name: Wait for Device Boot
      run: sleep 5
    
    - name: Get Device IP
      id: device
      run: |
        # Assuming mDNS or static IP
        echo "ip=192.168.1.100" >> $GITHUB_OUTPUT
    
    - name: Run Hardware Tests
      run: |
        DEVICE_IP=${{ steps.device.outputs.ip }} \
        ./test/integration/run_telnet_tests.sh
```

---

## 8. Implementation Roadmap

### Phase 1: Foundation (Week 1-2)

- [ ] Set up native test environment in `platformio.ini`
- [ ] Create mock objects for WiFiClient, WiFiServer
- [ ] Extract `CommandParser` class from `RemoteDebug.cpp`
- [ ] Write unit tests for `CommandParser`
- [ ] Add test run to CI pipeline

### Phase 2: Core Logic Tests (Week 3-4)

- [ ] Extract `MessageFormatter` class
- [ ] Extract `InputBuffer` class  
- [ ] Write unit tests for formatting and input handling
- [ ] Add level filtering tests
- [ ] Achieve 50%+ code coverage on extracted components

### Phase 3: Integration Tests (Week 5-6)

- [ ] Create test firmware (`test/firmware/test_firmware.ino`)
- [ ] Set up Wokwi configuration
- [ ] Write telnet integration test scripts
- [ ] Add Wokwi tests to CI pipeline
- [ ] Test password authentication flow

### Phase 4: Comprehensive Coverage (Week 7-8)

- [ ] Add WebSocket tests (if not disabled)
- [ ] Add edge case tests (long messages, special chars)
- [ ] Add performance/stress tests
- [ ] Document test procedures
- [ ] Create test result dashboard

---

## Summary

| Test Type | Purpose | Location | CI Integration |
|-----------|---------|----------|----------------|
| **Unit Tests** | Test pure logic in isolation | `test/unit/` | ✅ Every commit |
| **Integration Tests** | Test network communication | `test/integration/` | ✅ Via Wokwi |
| **Hardware Tests** | Full end-to-end validation | `test/firmware/` | Optional (self-hosted) |

### Recommended Starting Point

1. **Start with integration tests** - They provide the most value with the current code structure
2. **Create the test firmware** - A predictable test target makes automated testing reliable
3. **Add unit tests incrementally** - As code is refactored, extract testable components

The integration testing approach (telnet scripts against real/simulated device) gives you **immediate value** without requiring major refactoring, while unit tests become more valuable as you implement the architectural improvements in the roadmap.
