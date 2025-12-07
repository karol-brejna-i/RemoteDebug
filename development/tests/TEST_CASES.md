# RemoteDebug Test Cases

This document lists specific test cases for the RemoteDebug library, organized to support **regression testing first**, then new feature validation.

---

## Test Philosophy

**Regression tests come FIRST.** Before implementing any improvements, we need confidence that existing functionality works. This document is organized as:

1. **REGRESSION TESTS** (Sections 1-7) - Cover ALL existing functionality
2. **IMPROVEMENT TESTS** (Section 8) - Tests for planned new features

---

## How to Use This Document

Each test case includes:
- **ID**: Unique identifier for tracking
- **Description**: What the test verifies  
- **Preconditions**: Required setup
- **Steps**: How to execute the test
- **Expected Result**: What should happen
- **Priority**: Critical/High/Medium/Low
- **Regression**: ✅ = tests existing functionality

Use these test cases for:
1. **Regression testing** - Run before ANY code changes
2. **Manual testing** during development
3. **Automated test scripts** - `test/integration/run_tests.sh`
4. **Validating new features** after implementing improvements

---

## Test Case Summary

### Regression Tests (Existing Functionality)

| Category | Description | Count | Coverage |
|----------|-------------|-------|----------|
| [1. Connection](#1-connection-tests) | TCP/Telnet connection lifecycle | 10 | ✅ Complete |
| [2. Commands](#2-command-tests) | Every built-in telnet command | 32 | ✅ Complete |
| [3. Log Levels](#3-log-level-tests) | Level filtering & isActive() | 12 | ✅ Complete |
| [4. Authentication](#4-authentication-tests) | Password protection flow | 8 | ✅ Complete |
| [5. Output & Formatting](#5-output--formatting-tests) | Colors, time, profiler, serial | 12 | ✅ Complete |
| [6. API Methods](#6-api-method-tests) | Public API verification | 15 | ✅ Complete |
| [7. Edge Cases & Stability](#7-edge-cases--stability-tests) | Error handling, limits, memory | 12 | ✅ Complete |
| **Total Regression Tests** | | **101** | |

### New Feature Tests (Planned Improvements)

| Category | Description | Count |
|----------|-------------|-------|
| [8. Improvements](#8-improvement-tests) | Tests for planned features | Defined when implemented |

---

## 1. Connection Tests

> **Regression Coverage**: Tests core connection lifecycle that must work before/after any changes.

### TC-CON-001: Basic TCP Connection ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Device running with RemoteDebug initialized |
| **Steps** | 1. Run `nc <device_ip> 23` or `telnet <device_ip> 23` |
| **Expected Result** | Connection established, welcome message displayed |

### TC-CON-002: Welcome Message Content ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Device initialized with hostname "mydevice" |
| **Steps** | 1. Connect via telnet<br>2. Observe welcome message |
| **Expected Result** | Welcome message includes: hostname, IP, version, hint to press 'h' for help |

### TC-CON-003: Disconnect via Quit Command (q) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Connected to device |
| **Steps** | 1. Send `q` command |
| **Expected Result** | "Closing client connection" message, connection closed |

### TC-CON-004: Reconnection After Quit ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Previously connected and quit |
| **Steps** | 1. Connect again via telnet |
| **Expected Result** | New connection accepted, welcome message shown |

### TC-CON-005: Same IP Reconnection ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected to device |
| **Steps** | 1. Kill telnet process abruptly<br>2. Reconnect from same IP |
| **Expected Result** | Connection accepted (old session replaced) |

### TC-CON-006: Different IP Connection Rejected ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Device connected from IP-A |
| **Steps** | 1. Attempt connection from different IP-B |
| **Expected Result** | Second connection rejected (only one client allowed) |

### TC-CON-007: Inactivity Timeout ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Device with MAX_TIME_INACTIVE set (default 5 min) |
| **Steps** | 1. Connect<br>2. Wait without sending commands for MAX_TIME_INACTIVE + buffer |
| **Expected Result** | "Closing session by inactivity" message, disconnected |

### TC-CON-008: Activity Resets Timeout ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Connected, inactivity timer running |
| **Steps** | 1. Send any command before timeout<br>2. Wait another full timeout period |
| **Expected Result** | Timer resets on command, disconnect only after full inactivity period |

### TC-CON-009: isConnected() API ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Device running |
| **Steps** | 1. Check `Debug.isConnected()` before connection<br>2. Connect<br>3. Check again<br>4. Disconnect<br>5. Check again |
| **Expected Result** | Returns false → true → false |

### TC-CON-010: handle() Called in Loop ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Device running, `Debug.handle()` in loop() |
| **Steps** | 1. Connect<br>2. Send commands<br>3. Observe responses |
| **Expected Result** | Commands processed, responses received (handle() is processing) |

---

## 2. Command Tests

> **Regression Coverage**: Tests EVERY built-in telnet command from RemoteDebug.cpp processCommand()

### 2.1 Help & Info Commands

### TC-CMD-001: Help Command (h) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Connected |
| **Steps** | 1. Send `h` |
| **Expected Result** | Help text displayed with list of all available commands |

### TC-CMD-002: Help Command (?) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Connected |
| **Steps** | 1. Send `?` |
| **Expected Result** | Same help text as `h` command |

### TC-CMD-003: Memory Command (m) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Connected |
| **Steps** | 1. Send `m` |
| **Expected Result** | "Free Heap RAM: [number]" displayed |

### 2.2 Debug Level Commands

### TC-CMD-004: Set Verbose Level (v) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Connected |
| **Steps** | 1. Send `v` |
| **Expected Result** | "Debug level set to Verbose" |

### TC-CMD-005: Set Debug Level (d) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Connected |
| **Steps** | 1. Send `d` |
| **Expected Result** | "Debug level set to Debug" |

### TC-CMD-006: Set Info Level (i) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Connected |
| **Steps** | 1. Send `i` |
| **Expected Result** | "Debug level set to Info" |

### TC-CMD-007: Set Warning Level (w) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Connected |
| **Steps** | 1. Send `w` |
| **Expected Result** | "Debug level set to Warning" |

### TC-CMD-008: Set Error Level (e) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Connected |
| **Steps** | 1. Send `e` |
| **Expected Result** | "Debug level set to Error" |

### TC-CMD-009: Toggle Debug Level Display (l) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send `l` |
| **Expected Result** | "Show debug level: On" or "Show debug level: Off" (toggles) |

### 2.3 Display & Formatting Commands

### TC-CMD-010: Toggle Time Display (t) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send `t` |
| **Expected Result** | "Show time: On" or "Show time: Off" (toggles) |

### TC-CMD-011: Toggle Colors (c) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send `c` |
| **Expected Result** | "Show colors: On" or "Show colors: Off" (toggles) |

### TC-CMD-012: Toggle Profiler (p) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send `p` |
| **Expected Result** | "Show profiler: On" or "Show profiler: Off" (toggles) |

### TC-CMD-013: Profiler with Minimum Time (p N) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Connected |
| **Steps** | 1. Send `p 100` |
| **Expected Result** | "Show profiler: On (with minimal time: 100)" |

### TC-CMD-014: Profiler Debug Level (P) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Connected |
| **Steps** | 1. Send `P` |
| **Expected Result** | "Debug level set to Profiler (disable in [N] millis)" |

### TC-CMD-015: Auto Profiler Level (A) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Connected |
| **Steps** | 1. Send `A` |
| **Expected Result** | "Auto profiler debug level active (time >= [N] millis)" |

### 2.4 Silence & Filter Commands

### TC-CMD-016: Toggle Silence Mode (s) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected, debug messages flowing |
| **Steps** | 1. Send `s` |
| **Expected Result** | "Debug now is in silent mode!" and output stops |

### TC-CMD-017: Exit Silence with Any Command ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | In silence mode |
| **Steps** | 1. Send any command other than `s` (e.g., `h`) |
| **Expected Result** | "Debug now exit from silent mode!" and output resumes |

### TC-CMD-018: Set Filter (filter <text>) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send `filter HEARTBEAT` |
| **Expected Result** | "Filter active: heartbeat" (converted to lowercase) |

### TC-CMD-019: Filter Case Insensitive ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Connected |
| **Steps** | 1. Send `filter TEST`<br>2. Trigger messages containing "test", "Test", "TEST" |
| **Expected Result** | All messages containing the pattern shown (case insensitive) |

### TC-CMD-020: Clear Filter (nofilter) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Filter active |
| **Steps** | 1. Send `nofilter` |
| **Expected Result** | "Filter disabled", all messages shown again |

### 2.5 Timeout Command

### TC-CMD-021: Get Connection Timeout (timeout) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send `timeout` |
| **Expected Result** | "Connection Timeout: [N] seconds (0=disabled)" |

### TC-CMD-022: Set Connection Timeout (timeout N) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send `timeout 120` |
| **Expected Result** | "Connection Timeout: 120 seconds" |

### TC-CMD-023: Timeout Minimum Validation ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Connected |
| **Steps** | 1. Send `timeout 30` (less than 60) |
| **Expected Result** | "Connection Timeout must be minimal 60 seconds" |

### TC-CMD-024: Timeout Zero (Disable) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Connected |
| **Steps** | 1. Send `timeout 0` |
| **Expected Result** | "Connection Timeout: 0 seconds (0=disabled)" |

### 2.6 System Commands

### TC-CMD-025: Quit Command (q) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Connected |
| **Steps** | 1. Send `q` |
| **Expected Result** | "Closing client connection ...", connection closed |

### TC-CMD-026: Reset Command (reset) - Enabled ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | `setResetCmdEnabled(true)` called |
| **Steps** | 1. Send `reset` |
| **Expected Result** | "Resetting the ESP32/ESP8266...", device restarts |

### TC-CMD-027: Reset Command (reset) - Disabled ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | `setResetCmdEnabled(false)` or not called (default) |
| **Steps** | 1. Send `reset` |
| **Expected Result** | Command ignored (no reset occurs) |

### TC-CMD-028: CPU 80MHz (cpu80) - ESP8266 Only ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Low |
| **Preconditions** | Connected to ESP8266 device |
| **Steps** | 1. Send `cpu80` |
| **Expected Result** | "CPU ESP8266 changed to: 80 MHz" |

### TC-CMD-029: CPU 160MHz (cpu160) - ESP8266 Only ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Low |
| **Preconditions** | Connected to ESP8266 device |
| **Steps** | 1. Send `cpu160` |
| **Expected Result** | "CPU ESP8266 changed to: 160 MHz" |

### 2.7 Unknown & Edge Cases

### TC-CMD-030: Unknown Command ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send `xyz_unknown_command_123` |
| **Expected Result** | No crash; command passed to callback if set, otherwise ignored |

### TC-CMD-031: Empty Command (just Enter) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Connected |
| **Steps** | 1. Press Enter without typing command |
| **Expected Result** | No crash, no output (command length is 0, not processed) |

### TC-CMD-032: Command with Spaces ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Connected |
| **Steps** | 1. Send `filter test pattern with spaces` |
| **Expected Result** | Filter set to "test pattern with spaces" |

---

## 3. Log Level Tests

> **Regression Coverage**: Tests debug level filtering logic - core functionality.

### TC-LVL-001: Verbose Shows All Levels ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Connected, level set to VERBOSE (`v`) |
| **Steps** | 1. Send `v`<br>2. Trigger messages at all levels (V, D, I, W, E) |
| **Expected Result** | All messages shown |

### TC-LVL-002: Debug Filters Verbose ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Connected |
| **Steps** | 1. Send `d`<br>2. Trigger debugV() and debugD() messages |
| **Expected Result** | VERBOSE hidden, DEBUG and higher shown |

### TC-LVL-003: Info Filters Verbose and Debug ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Connected |
| **Steps** | 1. Send `i`<br>2. Trigger V, D, I, W, E messages |
| **Expected Result** | Only INFO, WARNING, ERROR shown |

### TC-LVL-004: Warning Filters V, D, I ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Connected |
| **Steps** | 1. Send `w`<br>2. Trigger all level messages |
| **Expected Result** | Only WARNING and ERROR shown |

### TC-LVL-005: Error Shows Only Errors ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Connected |
| **Steps** | 1. Send `e`<br>2. Trigger all level messages |
| **Expected Result** | Only ERROR messages shown |

### TC-LVL-006: isActive(VERBOSE) Returns Correct Value ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Level set to DEBUG |
| **Steps** | 1. In firmware: check `Debug.isActive(Debug.VERBOSE)` |
| **Expected Result** | Returns `false` |

### TC-LVL-007: isActive(ERROR) Always True ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Any level set (even VERBOSE) |
| **Steps** | 1. In firmware: check `Debug.isActive(Debug.ERROR)` |
| **Expected Result** | Returns `true` |

### TC-LVL-008: isActive(ANY) Always True ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Any level set |
| **Steps** | 1. In firmware: check `Debug.isActive(Debug.ANY)` |
| **Expected Result** | Returns `true` |

### TC-LVL-009: Debug Level Prefix Display ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Level display enabled (default) |
| **Steps** | 1. Trigger messages at each level |
| **Expected Result** | Prefix shown: (P), (V), (D), (I), (W), (E) |

### TC-LVL-010: Toggle Level Display (l) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send `l` to toggle off<br>2. Trigger message<br>3. Send `l` to toggle on<br>4. Trigger message |
| **Expected Result** | Level prefix hidden then shown |

### TC-LVL-011: Starting Debug Level ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Device just booted |
| **Steps** | 1. Connect<br>2. Check what level messages are shown |
| **Expected Result** | Default level is DEBUG (or as specified in begin()) |

### TC-LVL-012: Level Persists During Session ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Connected, level set to WARNING |
| **Steps** | 1. Wait 2 minutes<br>2. Verify only W/E messages shown |
| **Expected Result** | Level setting persists |

---

## 4. Authentication Tests

> **Regression Coverage**: Tests password protection feature.

### TC-AUTH-001: No Password - Direct Access ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | `setPassword()` NOT called |
| **Steps** | 1. Connect via telnet |
| **Expected Result** | Welcome message and immediate access to commands |

### TC-AUTH-002: Password Set - Prompt Shown ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | `Debug.setPassword("secret123")` called |
| **Steps** | 1. Connect via telnet |
| **Expected Result** | Password prompt shown, "?" or other text indicates password needed |

### TC-AUTH-003: Correct Password Grants Access ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Password set to "secret123" |
| **Steps** | 1. Connect<br>2. Enter "secret123" |
| **Expected Result** | "Password ok, allowing access now...", help shown |

### TC-AUTH-004: Wrong Password Rejected ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Password set |
| **Steps** | 1. Connect<br>2. Enter "wrongpassword" |
| **Expected Result** | "Wrong password!" message |

### TC-AUTH-005: Multiple Wrong Attempts - Lockout ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Password set, REMOTEDEBUG_PWD_ATTEMPTS defined (default 3) |
| **Steps** | 1. Connect<br>2. Enter wrong password 4 times |
| **Expected Result** | "Many attempts. Closing session now.", disconnected |

### TC-AUTH-006: Password Timeout ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Password set, PASSWORD_TIMEOUT_MS active (default 60s) |
| **Steps** | 1. Connect<br>2. Wait 60+ seconds without entering password |
| **Expected Result** | Disconnected due to password timeout |

### TC-AUTH-007: Commands Before Auth Treated as Password ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Password set, not authenticated |
| **Steps** | 1. Connect<br>2. Send `h` (help command) |
| **Expected Result** | "Wrong password!" (command treated as password attempt) |

### TC-AUTH-008: Password with Spaces ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Password set to "my secret pass" |
| **Steps** | 1. Connect<br>2. Enter "my secret pass" |
| **Expected Result** | Password accepted (spaces preserved) |

---

## 5. Output & Formatting Tests

> **Regression Coverage**: Tests output formatting options (colors, time, profiler).

### TC-FMT-001: Colors Disabled by Default ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Fresh connection, `showColors()` not called |
| **Steps** | 1. Trigger debug message |
| **Expected Result** | No ANSI color codes in output |

### TC-FMT-002: Colors Enabled via showColors(true) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | `Debug.showColors(true)` called in setup |
| **Steps** | 1. Connect<br>2. Trigger messages at different levels |
| **Expected Result** | ANSI codes present (\\x1B[...m) |

### TC-FMT-003: Colors Toggle via Command (c) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send `c`<br>2. Observe confirmation |
| **Expected Result** | "Show colors: On" or "Show colors: Off" |

### TC-FMT-004: Time Display via showTime(true) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | `Debug.showTime(true)` called |
| **Steps** | 1. Trigger debug message |
| **Expected Result** | Timestamp included in message |

### TC-FMT-005: Time Toggle via Command (t) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send `t`<br>2. Observe confirmation |
| **Expected Result** | "Show time: On" or "Show time: Off" |

### TC-FMT-006: Profiler Display via showProfiler(true) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | `Debug.showProfiler(true)` called |
| **Steps** | 1. Trigger debug messages with delays |
| **Expected Result** | Time delta shown between messages |

### TC-FMT-007: Profiler Toggle via Command (p) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send `p` |
| **Expected Result** | "Show profiler: On" or "Show profiler: Off" |

### TC-FMT-008: Function Name Auto-Display ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | DEBUG_DISABLE_AUTO_FUNC not defined (default) |
| **Steps** | 1. Use debugV() from function named `myTestFunc` |
| **Expected Result** | Output includes "(myTestFunc)" |

### TC-FMT-009: Core ID Display - ESP32 ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | ESP32, DEBUG_AUTO_CORE defined |
| **Steps** | 1. Trigger debug message |
| **Expected Result** | Output includes "(C0)" or "(C1)" for core ID |

### TC-FMT-010: Serial Mirror via setSerialEnabled(true) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | `Debug.setSerialEnabled(true)` called |
| **Steps** | 1. Monitor Serial output<br>2. Connect via telnet<br>3. Trigger debug message |
| **Expected Result** | Message appears on both Serial AND telnet |

### TC-FMT-011: Newline Handling - debugXln Macros ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Use `debugI("msg1")` then `debugI("msg2")` |
| **Expected Result** | Each on separate line (\\n appended) |

### TC-FMT-012: printf-style Formatting ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Use `debugI("value: %d, string: %s", 42, "test")` |
| **Expected Result** | Output: "value: 42, string: test" |

---

## 6. API Method Tests

> **Regression Coverage**: Tests public API methods of RemoteDebug class.
>
> **Automated Tests**: The following are automated via `run_tests.sh` with `TEST_FIRMWARE=1`:
> - TC-API-006: `getLastCommand()` - via `test_last_cmd` command
> - TC-API-007: `clearLastCommand()` - via `test_clear_cmd` command
> - TC-API-008: `setCallBackProjectCmds()` - via `test_callback` command
> - TC-API-009: `isConnected()` - via `test_connected` command
> - TC-API-014: `isSilence()` - via `test_silence` command

### TC-API-001: begin(hostname) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | WiFi connected |
| **Steps** | 1. Call `Debug.begin("mydevice")` |
| **Expected Result** | Returns true, telnet server started on port 23 |

### TC-API-002: begin(hostname, port) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | WiFi connected |
| **Steps** | 1. Call `Debug.begin("mydevice", 2323)` |
| **Expected Result** | Telnet server on specified port (NOTE: currently buggy - see improvement 3.7) |

### TC-API-003: begin(hostname, startingLevel) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | WiFi connected |
| **Steps** | 1. Call `Debug.begin("mydevice", Debug.INFO)` |
| **Expected Result** | Initial debug level is INFO |

### TC-API-004: stop() ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Debug started, client connected |
| **Steps** | 1. Call `Debug.stop()` |
| **Expected Result** | Connection closed, telnet server stopped |

### TC-API-005: handle() Called Regularly ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Critical |
| **Preconditions** | Debug started |
| **Steps** | 1. Ensure `Debug.handle()` is in loop()<br>2. Connect and send commands |
| **Expected Result** | Commands processed, responses sent |

### TC-API-006: getLastCommand() ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected, command sent |
| **Steps** | 1. Send "mycommand"<br>2. In callback, call `Debug.getLastCommand()` |
| **Expected Result** | Returns "mycommand" |

### TC-API-007: clearLastCommand() ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Command received |
| **Steps** | 1. Call `Debug.clearLastCommand()`<br>2. Call `Debug.getLastCommand()` |
| **Expected Result** | Returns empty string |

### TC-API-008: setHelpProjectsCmds() ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Debug initialized |
| **Steps** | 1. Call `Debug.setHelpProjectsCmds("mycmd - do something")`<br>2. Connect<br>3. Send `h` |
| **Expected Result** | Help text includes "mycmd - do something" |

### TC-API-009: setCallBackProjectCmds() ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Debug initialized |
| **Steps** | 1. Set callback: `Debug.setCallBackProjectCmds(myCallback)`<br>2. Connect<br>3. Send unknown command "mycmd" |
| **Expected Result** | Callback function is invoked |

### TC-API-010: setCallBackNewClient() ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Debug initialized |
| **Steps** | 1. Set callback: `Debug.setCallBackNewClient(onConnect)`<br>2. Connect via telnet |
| **Expected Result** | Callback invoked on new connection |

### TC-API-011: setFilter() Programmatic ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Debug initialized |
| **Steps** | 1. Call `Debug.setFilter("pattern")`<br>2. Trigger messages with/without pattern |
| **Expected Result** | Only matching messages shown |

### TC-API-012: setNoFilter() Programmatic ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Filter active |
| **Steps** | 1. Call `Debug.setNoFilter()` |
| **Expected Result** | Filter disabled, all messages shown |

### TC-API-013: silence() Programmatic ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Connected |
| **Steps** | 1. Call `Debug.silence(true)` |
| **Expected Result** | Output silenced |

### TC-API-014: isSilence() ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Debug running |
| **Steps** | 1. Check `Debug.isSilence()`<br>2. Enable silence<br>3. Check again |
| **Expected Result** | Returns false, then true |

### TC-API-015: disconnect() Programmatic ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Client connected |
| **Steps** | 1. Call `Debug.disconnect()` |
| **Expected Result** | Client disconnected, message sent |

---

## 7. Edge Cases & Stability Tests

> **Regression Coverage**: Tests error handling and edge conditions.

### TC-EDGE-001: Long Message (500+ chars) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send debug message > 500 characters |
| **Expected Result** | Message transmitted without crash |

### TC-EDGE-002: Special Characters in Message ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send message with \\t, \\n, quotes, backslashes |
| **Expected Result** | Characters displayed correctly, no crash |

### TC-EDGE-003: Rapid Commands ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send 10 commands in <1 second |
| **Expected Result** | All processed (with duplicate filter active) |

### TC-EDGE-004: Message Flood (100+) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Trigger 100 debug messages rapidly |
| **Expected Result** | Device remains responsive |

### TC-EDGE-005: Binary/NULL Input ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Connected |
| **Steps** | 1. Send bytes: 0x00, 0xFF, 0x1B |
| **Expected Result** | No crash, non-printable ignored |

### TC-EDGE-006: Very Long Command (500+ chars) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Connected |
| **Steps** | 1. Send command string > 500 chars |
| **Expected Result** | No crash, command handled or truncated |

### TC-EDGE-007: Reconnect Stress (50 cycles) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Device running |
| **Steps** | 1. Connect-disconnect 50 times |
| **Expected Result** | No memory leak, all connections handled |

### TC-EDGE-008: Abrupt Disconnect ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Kill telnet process abruptly (not using `q`) |
| **Expected Result** | Device detects disconnect, cleans up |

### TC-EDGE-009: Duplicate Command Filter ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Connected |
| **Steps** | 1. Send same command twice in <50ms |
| **Expected Result** | Command processed once (bug workaround active) |

### TC-EDGE-010: CRLF vs LF Handling ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Connected |
| **Steps** | 1. Send command with \\r\\n<br>2. Send command with just \\n |
| **Expected Result** | Both work correctly, no double processing |

### TC-EDGE-011: Memory Stability ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | High |
| **Preconditions** | Device running |
| **Steps** | 1. Record heap<br>2. 100 commands<br>3. Check heap |
| **Expected Result** | Heap difference < 1KB (no leak) |

### TC-EDGE-012: Long Session (1 hour) ✅ Regression
| Field | Value |
|-------|-------|
| **Priority** | Medium |
| **Preconditions** | Active debug output |
| **Steps** | 1. Stay connected 1 hour<br>2. Periodically interact |
| **Expected Result** | No degradation, stable memory |

---

## 8. Improvement-Specific Tests (New Features)

> **NOT regression** - Tests for planned improvements. Add these AFTER implementing features.

### For Improvement 2.1: Separate Connection Handlers
| Test ID | Description |
|---------|-------------|
| TC-IMP-2.1-001 | IConnectionHandler interface works with TelnetHandler |
| TC-IMP-2.1-002 | IConnectionHandler interface works with WebSocketHandler |
| TC-IMP-2.1-003 | Switching handlers at runtime |

### For Improvement 3.2: Compile-Time Log Levels  
| Test ID | Description |
|---------|-------------|
| TC-IMP-3.2-001 | DEBUG_MIN_LEVEL=INFO removes V/D from binary |
| TC-IMP-3.2-002 | Binary size reduction with higher min level |
| TC-IMP-3.2-003 | isActive() returns false for compiled-out levels |

### For Improvement 3.3: Log Ring Buffer
| Test ID | Description |
|---------|-------------|
| TC-IMP-3.3-001 | Messages before connection are buffered |
| TC-IMP-3.3-002 | Buffer sent on new connection |
| TC-IMP-3.3-003 | Buffer overflow evicts oldest messages |
| TC-IMP-3.3-004 | Buffer size is configurable |

### For Improvement 3.7: Configurable Port Numbers
| Test ID | Description |
|---------|-------------|
| TC-IMP-3.7-001 | begin(host, 2323) starts on port 2323 |
| TC-IMP-3.7-002 | Connection works on custom port |

### For Improvement 6.1: Improve Password Security
| Test ID | Description |
|---------|-------------|
| TC-IMP-6.1-001 | Password not echoed in terminal |
| TC-IMP-6.1-002 | Rate limiting on password attempts |
| TC-IMP-6.1-003 | Challenge-response authentication |

---

## Automated Test Matrix

### Currently Automated in run_tests.sh

| Test ID | Implemented | Status |
|---------|-------------|--------|
| TC-CON-001 | ✅ | Connection test |
| TC-CMD-001 | ✅ | Help (h) |
| TC-CMD-002 | ✅ | Help (?) |
| TC-CMD-003 | ✅ | Memory (m) |
| TC-CMD-004-008 | ✅ | Level commands |
| TC-CMD-010 | ✅ | Time toggle |
| TC-CMD-011 | ✅ | Color toggle |
| TC-CMD-012 | ✅ | Profiler toggle |
| TC-CMD-016 | ✅ | Silence |
| TC-EDGE-003 | ✅ | Rapid commands |

### To Be Automated

| Test ID | Priority |
|---------|----------|
| TC-AUTH-* | High - Add password test mode |
| TC-CMD-018-020 | High - Filter commands |
| TC-CMD-021-024 | Medium - Timeout commands |
| TC-EDGE-001-006 | Medium - Edge cases |

---

## Regression Test Checklist

Before ANY code change, run these minimum regression tests:

### Quick Smoke Test (5 min)
- [ ] TC-CON-001: Can connect
- [ ] TC-CMD-001: Help works
- [ ] TC-CMD-003: Memory command works
- [ ] TC-CMD-004-008: Level commands work
- [ ] TC-CMD-025: Quit works

### Standard Regression (15 min)
- [ ] All Quick Smoke tests
- [ ] TC-CMD-010-012: Toggle commands
- [ ] TC-CMD-018, 020: Filter set/clear
- [ ] TC-LVL-001-005: Level filtering
- [ ] TC-FMT-003, 005, 007: Format toggles

### Full Regression (45 min)
- [ ] All Standard tests
- [ ] TC-AUTH-001-005: If password feature used
- [ ] TC-API-001-015: API methods
- [ ] TC-EDGE-001-012: Edge cases
