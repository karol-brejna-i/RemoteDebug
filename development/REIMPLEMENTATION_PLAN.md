# RemoteDebug Library Analysis and Reimplementation Plan

## Executive Summary

RemoteDebug is an Arduino library for ESP8266/ESP32 that enables remote debugging over WiFi via telnet or WebSocket connections. It provides debug levels, profiling, custom commands, and a web-based debug interface.

---

## 1. Current Architecture Overview

### 1.1 Core Components

| Component | File | Purpose |
|-----------|------|---------|
| `RemoteDebug` | `src/RemoteDebug.h`, `src/RemoteDebug.cpp` | Main class - telnet server, debug output, command processing |
| `RemoteDebugWS` | `src/RemoteDebugWS.h`, `src/RemoteDebugWS.cpp` | WebSocket server for RemoteDebugApp |
| `RemoteDebugCfg` | `src/RemoteDebugCfg.h` | Configuration defines |
| `telnet.h` | `src/telnet.h` | Telnet protocol constants |

### 1.2 Class Hierarchy

```
Print (Arduino base class)
├── RemoteDebug (main class)
└── RemoteDebugWS (WebSocket handler)

RemoteDebugWSCallbacks (abstract callback interface)
└── MyRemoteDebugCallbacks (internal implementation)
```

---

## 2. Functionality Analysis

### 2.1 Debug Levels

| Level | Value | Purpose |
|-------|-------|---------|
| `PROFILER` | 0 | Execution timing |
| `VERBOSE` | 1 | Verbose messages |
| `DEBUG` | 2 | Debug messages |
| `INFO` | 3 | Info messages |
| `WARNING` | 4 | Warning messages |
| `ERROR` | 5 | Error messages |
| `ANY` | 6 | Always shown |

### 2.2 Debug Macros

```cpp
// Primary macros (with auto newline)
debugV(fmt, ...)  // Verbose
debugD(fmt, ...)  // Debug
debugI(fmt, ...)  // Info
debugW(fmt, ...)  // Warning
debugE(fmt, ...)  // Error
debugA(fmt, ...)  // Always

// Raw print macros (for multi-statement output)
rprintV(x, ...), rprintVln(x, ...)
rprintD(x, ...), rprintDln(x, ...)
// ... etc for each level
```

### 2.3 Core Public API

```cpp
class RemoteDebug : public Print {
public:
    // Initialization
    bool begin(String hostName, uint16_t port = 23, uint8_t startingDebugLevel = DEBUG);
    void stop();
    void handle();  // Must be called in loop()
    
    // Connection
    void disconnect(boolean onlyTelnetClient = false);
    void setPassword(String password);
    
    // Output configuration
    void setSerialEnabled(boolean enable);
    void showTime(boolean show);
    void showProfiler(boolean show, uint32_t minTime = 0);
    void showDebugLevel(boolean show);
    void showColors(boolean show);
    void showRaw(boolean show);
    
    // Commands
    void setResetCmdEnabled(boolean enable);
    void setHelpProjectsCmds(String help);
    void setCallBackProjectCmds(void (*callback)());
    void setCallBackNewClient(void (*callback)());
    String getLastCommand();
    void clearLastCommand();
    
    // Filtering
    void setFilter(String filter);
    void setNoFilter();
    
    // State
    boolean isActive(uint8_t debugLevel);
    boolean isConnected();
    
    // Print interface
    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t* buffer, size_t size);
};
```

---

## 3. Communication Protocols

### 3.1 Telnet Protocol

**Port:** 23 (configurable via `TELNET_PORT` in `src/RemoteDebugCfg.h`)

**Connection Flow:**
1. Client connects to TCP port 23
2. If password set: send password prompt, validate input
3. Send welcome message with help (if `SHOW_HELP` enabled)
4. Enter command/output loop

**Telnet Commands:**

| Command | Description |
|---------|-------------|
| `?` or `help` | Show help |
| `q` | Quit connection |
| `m` | Show free memory |
| `v` | Set level to VERBOSE |
| `d` | Set level to DEBUG |
| `i` | Set level to INFO |
| `w` | Set level to WARNING |
| `e` | Set level to ERROR |
| `s` | Toggle silence mode |
| `l` | Toggle show debug level |
| `t` | Toggle show time |
| `timeout <sec>` | Set connection timeout |
| `p` | Toggle profiler |
| `p <min>` | Profiler with minimum time |
| `P <time>` | Set profiler level for duration |
| `c` | Toggle colors |
| `filter <text>` | Set output filter |
| `nofilter` | Remove filter |
| `reset` | Reset ESP (if enabled) |
| `cpu80` / `cpu160` | Change CPU frequency (ESP8266) |

**Output Format:**

Standard debug output line:
```
[COLOR_CODE](LEVEL p:ELAPSED FUNCTION CORE) MESSAGE[RESET_CODE]
```

Example:
```
(V p:3065 loop C1) * Run time: 00:41:23 (VERBOSE)
```

### 3.2 WebSocket Protocol

**Port:** 8232 (from WebSocketsServer initialization)

#### App → Device Messages:

| Message | Description |
|---------|-------------|
| `$app` | Initial connection request |
| Regular commands | Same as telnet (v, d, i, etc.) |

#### Device → App Messages:

| Format | Description |
|--------|-------------|
| `$app:I` | Initial connection acknowledgment |
| `$app:V:<version>:<board>:<features>:<memory>:<dbgEnabled>:N` | Version/info response |
| `$app:L:<level>` | Current debug level |
| `$app:M:<memory>:` | Memory status |
| Regular text | Debug output |

**Message Field Details:**

```cpp
// $app:V:<version>:<board>:<features>:<memory>:<dbgEnabled>:N
// version: e.g., "4.0.0"
// board: "ESP32" or "ESP8266"
// features: 'E' (Enough/debugger enabled) or 'M' (Medium/debugger disabled)
// memory: free heap in bytes
// dbgEnabled: 'E' (enabled) or 'D' (disabled)
```

### 3.3 Client Buffering

From `src/RemoteDebugCfg.h`:

```cpp
#define CLIENT_BUFFERING true
#define DELAY_TO_SEND 10      // ms between sends
#define MAX_SIZE_SEND 1460    // Max packet size (TCP/IP limit)
```

---

## 4. Configuration Options

| Define | Default | Description |
|--------|---------|-------------|
| `DEBUG_DISABLED` | undefined | Compile out all debug code |
| `WEBSOCKET_DISABLED` | undefined | Disable WebSocket support |
| `TELNET_PORT` | 23 | Telnet server port |
| `MAX_TIME_INACTIVE` | 600000 | Inactivity timeout (10 min) |
| `BUFFER_PRINT` | 150 | Print buffer size |
| `SHOW_HELP` | true | Show help on connect |
| `CLIENT_BUFFERING` | true | Enable output buffering |
| `REMOTEDEBUG_PWD_ATTEMPTS` | 3 | Max password attempts |

---

## 5. State Management

### 5.1 Connection State

```cpp
boolean _connected = false;          // Telnet client connected
boolean _connectedWS = false;        // WebSocket client connected (static in .cpp)
boolean _passwordOk = false;         // Password validated
uint8_t _passwordAttempt = 0;        // Failed attempts counter
boolean _silence = false;            // Silence mode active
uint32_t _silenceTimeout = 0;        // Silence timeout
```

### 5.2 Debug State

```cpp
uint8_t _clientDebugLevel = DEBUG;   // Current debug level
uint8_t _lastDebugLevel = DEBUG;     // Last active level
uint32_t _lastTimePrint = millis();  // For profiler
boolean _showTime = false;           // Show timestamp
boolean _showProfiler = false;       // Show profiler info
uint32_t _minTimeShowProfiler = 0;   // Profiler threshold
boolean _showDebugLevel = true;      // Show level indicator
boolean _showColors = false;         // ANSI colors
boolean _showRaw = false;            // Raw output mode
boolean _serialEnabled = false;      // Echo to Serial
```

---

## 6. Debugger Integration (RemoteDebugger)

The library supports an external debugger addon via callbacks:

```cpp
void initDebugger(
    boolean (*callbackEnabled)(),           // Is debugger enabled?
    void (*callbackHandle)(const boolean),  // Handle debugger
    String (*callbackGetHelp)(),            // Get debugger help text
    void (*callbackProcessCmd)()            // Process debugger commands
);
```

---

## 7. Identified Issues in Current Implementation

### 7.1 Memory Inefficiencies

1. **Excessive String usage** - Many operations use Arduino `String` class which causes heap fragmentation
2. **Large help text** - Built dynamically on each request
3. **Buffer duplication** - Print buffer and send buffer maintained separately

### 7.2 Performance Issues

1. **Blocking operations** - `handle()` processes all input synchronously
2. **No async WebSocket** - Uses synchronous WebSocket library
3. **Polling-based** - Must call `handle()` frequently

### 7.3 Architecture Issues

1. **Tight coupling** - Telnet and WebSocket code intermingled
2. **Global state** - Static variables for WebSocket state
3. **Single client limit** - Only one connection at a time

---

## 8. Reimplementation Plan

### 8.1 Design Goals

1. **Memory efficiency** - Minimize heap allocations, use fixed buffers
2. **Performance** - Non-blocking operations where possible
3. **Modularity** - Separate transport from debug logic
4. **Compatibility** - Maintain API compatibility

### 8.2 Proposed Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    RemoteDebug                          │
│  (Public API - maintains backward compatibility)        │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────┴───────────────────────────────────────┐
│                  DebugCore                              │
│  - Debug levels                                         │
│  - Formatting                                           │
│  - Filtering                                            │
│  - Command parsing                                      │
└─────────────────┬───────────────────────────────────────┘
                  │
        ┌─────────┴─────────┐
        │                   │
┌───────┴───────┐   ┌───────┴───────┐
│ TelnetTransport│   │   WSTransport │
│               │   │               │
└───────────────┘   └───────────────┘
```

### 8.3 New Components

#### 8.3.1 DebugCore (Internal)

```cpp
class DebugCore {
public:
    // Level management
    void setLevel(uint8_t level);
    uint8_t getLevel() const;
    bool isActive(uint8_t level) const;
    
    // Output formatting
    size_t format(char* buffer, size_t size, uint8_t level, 
                  const char* func, const char* fmt, va_list args);
    
    // Command processing
    enum class Command { 
        NONE, HELP, QUIT, MEMORY, LEVEL_V, LEVEL_D, LEVEL_I, 
        LEVEL_W, LEVEL_E, SILENCE, SHOW_LEVEL, SHOW_TIME,
        TIMEOUT, PROFILER, COLORS, FILTER, NOFILTER, RESET,
        CPU80, CPU160, CUSTOM
    };
    Command parseCommand(const char* input, char* args, size_t argsSize);
    
    // State
    struct State {
        uint8_t level;
        bool showTime;
        bool showProfiler;
        uint32_t profilerMinTime;
        bool showLevel;
        bool showColors;
        bool silence;
        char filter[32];
    };
    
private:
    State _state;
    uint32_t _lastPrintTime;
};
```

#### 8.3.2 Transport Interface

```cpp
class IDebugTransport {
public:
    virtual ~IDebugTransport() = default;
    
    virtual bool begin(uint16_t port) = 0;
    virtual void stop() = 0;
    virtual void handle() = 0;
    
    virtual bool isConnected() const = 0;
    virtual void disconnect() = 0;
    
    virtual size_t write(const uint8_t* data, size_t len) = 0;
    virtual int available() = 0;
    virtual int read(char* buffer, size_t maxLen) = 0;
    
    // Callbacks
    using ConnectCallback = void (*)(bool connected);
    virtual void setConnectCallback(ConnectCallback cb) = 0;
};
```

#### 8.3.3 TelnetTransport

```cpp
class TelnetTransport : public IDebugTransport {
public:
    bool begin(uint16_t port = 23) override;
    void stop() override;
    void handle() override;
    
    // Password support
    void setPassword(const char* password);
    
private:
    WiFiServer _server;
    WiFiClient _client;
    char _password[32];
    uint8_t _passwordAttempts;
    bool _authenticated;
    
    // Ring buffer for output
    static constexpr size_t BUFFER_SIZE = 1460;
    char _txBuffer[BUFFER_SIZE];
    size_t _txHead, _txTail;
    
    void processNewConnection();
    void sendBuffered();
};
```

#### 8.3.4 WebSocketTransport

```cpp
class WebSocketTransport : public IDebugTransport {
public:
    bool begin(uint16_t port = 8232) override;
    void stop() override;
    void handle() override;
    
private:
    WebSocketsServer _ws;
    int8_t _clientNum;
    
    // Protocol handling
    void sendAppInfo();
    void sendLevelInfo();
    void onWebSocketEvent(uint8_t num, WStype_t type, 
                          uint8_t* payload, size_t length);
};
```

### 8.4 Memory Optimizations

1. **Fixed-size buffers** instead of String:
   ```cpp
   char _hostName[32];
   char _command[64];
   char _filter[32];
   char _helpProjectCmds[256];
   ```

2. **Flash strings for help text**:
   ```cpp
   static const char HELP_TEXT[] PROGMEM = "...";
   ```

3. **Ring buffer for output**:
   ```cpp
   template<size_t SIZE>
   class RingBuffer {
       char _buffer[SIZE];
       volatile size_t _head, _tail;
   public:
       size_t write(const char* data, size_t len);
       size_t read(char* data, size_t maxLen);
       size_t available() const;
       size_t free() const;
   };
   ```

### 8.5 API Compatibility Layer

The public `RemoteDebug` class maintains full backward compatibility:

```cpp
class RemoteDebug : public Print {
public:
    // Existing API - unchanged signatures
    bool begin(String hostName, uint16_t port = 23, 
               uint8_t startingDebugLevel = DEBUG);
    void handle();
    // ... all existing methods
    
private:
    DebugCore _core;
    TelnetTransport _telnet;
    WebSocketTransport _ws;  // Optional, controlled by define
};
```

### 8.6 Implementation Phases

#### Phase 1: Core Refactoring
- [ ] Implement `DebugCore` class
- [ ] Create `IDebugTransport` interface
- [ ] Implement `TelnetTransport`
- [ ] Unit tests for command parsing

#### Phase 2: WebSocket Support
- [ ] Implement `WebSocketTransport`
- [ ] Verify app protocol compatibility
- [ ] Test with RemoteDebugApp

#### Phase 3: Memory Optimization
- [ ] Replace String with fixed buffers
- [ ] Move help text to PROGMEM
- [ ] Implement ring buffer

#### Phase 4: Compatibility Testing
- [ ] Test all existing examples
- [ ] Verify macro compatibility
- [ ] Performance benchmarking
- [ ] Memory usage comparison

### 8.7 Breaking Changes (None Planned)

The reimplementation should be a **drop-in replacement**. No breaking changes to:
- Public API
- Debug macros
- Telnet commands
- WebSocket protocol

### 8.8 New Optional Features (Future)

1. **Multiple client support** (opt-in)
2. **Async WebSocket** (different library)
3. **UDP transport** (for logging without connection)
4. **Binary protocol** (for lower overhead)

---

## 9. File Structure for New Implementation

```
src/
├── RemoteDebug.h              # Public API (backward compatible)
├── RemoteDebug.cpp            # Facade implementation
├── RemoteDebugCfg.h           # Configuration (unchanged)
├── core/
│   ├── DebugCore.h            # Core debug logic
│   ├── DebugCore.cpp
│   ├── CommandParser.h        # Command parsing
│   └── CommandParser.cpp
├── transport/
│   ├── IDebugTransport.h      # Transport interface
│   ├── TelnetTransport.h
│   ├── TelnetTransport.cpp
│   ├── WebSocketTransport.h
│   └── WebSocketTransport.cpp
├── util/
│   ├── RingBuffer.h           # Ring buffer template
│   └── ProgmemStrings.h       # Flash string helpers
└── telnet.h                   # Telnet constants (unchanged)
```

---

## 10. Testing Strategy

### 10.1 Unit Tests
- Command parsing
- Level filtering
- Output formatting
- Ring buffer operations

### 10.2 Integration Tests
- Telnet connection/disconnection
- Password authentication
- WebSocket protocol
- Multiple reconnections

### 10.3 Compatibility Tests
- All three example sketches
- Macro behavior
- RemoteDebugApp connectivity

### 10.4 Performance Tests
- Memory usage comparison
- CPU overhead measurement
- Maximum message throughput

---

## 11. Documentation Deliverables

1. **API Reference** - Updated for any new optional features
2. **Migration Guide** - (Should be "no changes needed")
3. **Protocol Specification** - Formal documentation of telnet/WS protocols
4. **Performance Report** - Before/after comparison

---

## Appendix A: Current Source File Summary

| File | Lines | Description |
|------|-------|-------------|
| `src/RemoteDebug.cpp` | ~1400 | Main implementation |
| `src/RemoteDebug.h` | ~350 | Public API and macros |
| `src/RemoteDebugCfg.h` | ~100 | Configuration defines |
| `src/RemoteDebugWS.cpp` | ~200 | WebSocket implementation |
| `src/RemoteDebugWS.h` | ~80 | WebSocket header |
| `src/telnet.h` | ~50 | Telnet protocol constants |

---

*Document created: December 7, 2025*
*Based on analysis of RemoteDebug library v4.0.0*
