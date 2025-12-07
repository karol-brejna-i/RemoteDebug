# Plan for Improvement 7.1: Reduce String Allocations

## Problem Analysis

The codebase has **100+ `.concat()` calls** causing heap fragmentation:

| Area | Issue | Impact |
|------|-------|--------|
| `write()` function (lines 719-830) | Local `String show` with ~25 concat calls per debug line | **HOT PATH** - called for every character |
| `showHelp()` (lines 925-1040) | ~50 concat calls building help text | Called once per connection |
| `_bufferPrint` member | Frequent concat in character-by-character write | Moderate - already has `reserve()` |
| `_command` building | Concat per character in command input | Low frequency |

## Proposed Changes

### Phase 1: Optimize Hot Path (`write()` function) - HIGH PRIORITY

**1.1 Pre-allocate `show` string with `reserve()`**
```cpp
String show = "";
show.reserve(80);  // Typical prefix size: color(5) + level(2) + time(15) + profiler(25) + separator
```

**1.2 Use static/const strings for repeated values**
```cpp
// In header or as static const
static const char* const DEBUG_LEVEL_PREFIXES[] = {"(P", "(V", "(D", "(I", "(W", "(E"};
// Then: show.concat(DEBUG_LEVEL_PREFIXES[_lastDebugLevel]);
```

**1.3 Use `sprintf` for numeric formatting instead of multiple concat**
```cpp
// Instead of:
show.concat("t:");
show.concat(millis());
show.concat("ms");

// Use:
char timeBuf[20];
snprintf(timeBuf, sizeof(timeBuf), "t:%lums", millis());
show.concat(timeBuf);
```

### Phase 2: Optimize `showHelp()` - MEDIUM PRIORITY

**2.1 Use PROGMEM/F() for static help strings (ESP8266/ESP32)**
```cpp
static const char HELP_HEADER[] PROGMEM = "*** Remote debug - over telnet...";
```

**2.2 Pre-calculate help size and reserve**
```cpp
String help;
help.reserve(2048);  // Measured typical help size
```

**2.3 Consider sending help in chunks** instead of building one large string

### Phase 3: Member String Optimization - LOW PRIORITY

**3.1 Add `reserve()` calls in `begin()` for member strings:**
- `_command.reserve(64);`
- `_lastCommand.reserve(64);`
- `_bufferSend.reserve(MAX_SIZE_SEND);` (already done conditionally)

**3.2 Clear strings without deallocation:**
```cpp
// Instead of: _command = "";
// Use: _command.remove(0);  // or _command = ""  then _command.reserve(64);
```

## Implementation Order

| Step | Change | Files | Risk | Effort |
|------|--------|-------|------|--------|
| 1 | Add `reserve(80)` for `show` in `write()` | RemoteDebug.cpp | Low | 5 min |
| 2 | Create static prefix array for debug levels | RemoteDebug.cpp | Low | 15 min |
| 3 | Use snprintf for time/profiler formatting | RemoteDebug.cpp | Medium | 30 min |
| 4 | Add `reserve(2048)` in `showHelp()` | RemoteDebug.cpp | Low | 5 min |
| 5 | Add reserves for `_command`, `_lastCommand` in `begin()` | RemoteDebug.cpp | Low | 10 min |
| 6 | (Optional) Move help strings to PROGMEM | RemoteDebug.cpp | Medium | 1 hr |

## Expected Benefits

- **Reduced heap fragmentation**: Fewer allocations in hot path
- **Faster execution**: Pre-allocated buffers avoid realloc overhead
- **Lower memory peak**: Better memory reuse
- **Longer runtime stability**: Less fragmentation over time

## Testing Plan

1. Build verification on ESP8266 and ESP32
2. Monitor `ESP.getFreeHeap()` before/after changes
3. Run stress test with continuous debug output
4. Verify no functional regressions
