# WebSocketTransport Migration

## Overview

This document tracks the migration from `RemoteDebugWS` to `WebSocketTransport` - unifying the WebSocket transport to use the same `IDebugTransport` interface as `TelnetTransport`.

## Baseline (Before Migration)

### Binary Size

| Platform | RAM Used | Flash Used | Date |
|----------|----------|------------|------|
| **ESP32** | 47,268 bytes (14.4%) | 957,139 bytes (73.0%) | 2025-12-09 |
| **ESP8266** | 33,644 bytes (41.1%) | 301,981 bytes (28.9%) | 2025-12-09 |

### Current Architecture

Two WebSocket implementations exist:

| File | Class | Lines | Status |
|------|-------|-------|--------|
| `RemoteDebugWS.cpp/.h` | `RemoteDebugWS` | 317 | Production (active) |
| `WebSocketTransport.cpp/.h` | `WebSocketTransport` | 244 | New (unused) |

**Issues with current `RemoteDebugWS`:**
- Uses inheritance-based callbacks (`RemoteDebugWSCallbacks`)
- Different API than `TelnetTransport`
- Cannot be swapped/mocked for testing
- Extends `Print` class (adds complexity)

## Migration Goals

1. Replace `RemoteDebugWS` with `WebSocketTransport` in `RemoteDebug.cpp`
2. Remove `RemoteDebugWS.cpp/.h` and `MyRemoteDebugCallbacks` class
3. Achieve unified transport interface for both Telnet and WebSocket
4. Verify no functional regressions

## Expected Impact

| Metric | Expected Change |
|--------|-----------------|
| **Flash** | -2 to -4 KB (removing duplicate class + vtable) |
| **RAM** | -100 to -200 bytes |
| **Code complexity** | Reduced (unified interface) |
| **Testability** | Improved (mockable transport) |

## Changes Required

### Files to Modify
- `src/RemoteDebug.cpp` - Replace `DebugWS.*` calls with `_wsTransport.*`
- `src/RemoteDebug.h` - Add `WebSocketTransport _wsTransport` member

### Files to Remove
- `src/RemoteDebugWS.cpp`
- `src/RemoteDebugWS.h`

### API Mapping

| Old (`DebugWS.*`) | New (`_wsTransport.*`) |
|-------------------|------------------------|
| `DebugWS.begin(callbacks)` | `_wsTransport.begin(port)` + `setConnectCallback()` |
| `DebugWS.stop()` | `_wsTransport.stop()` |
| `DebugWS.handle()` | `_wsTransport.handle()` |
| `DebugWS.printf(fmt, ...)` | `_wsTransport.write()` with snprintf |
| `DebugWS.println(str)` | `_wsTransport.write()` |
| `DebugWS.print(str)` | `_wsTransport.write()` |
| `DebugWS.disconnect()` | `_wsTransport.disconnect()` |
| `DebugWS.isConnected()` | `_wsTransport.isConnected()` |

## Post-Migration Results

**Migration Date:** 2025-12-09

| Platform | Before RAM | After RAM | Before Flash | After Flash | RAM Δ | Flash Δ |
|----------|------------|-----------|--------------|-------------|-------|---------|
| **ESP32** | 47,268 (14.4%) | 45,980 (14.0%) | 957,139 (73.0%) | 959,711 (73.2%) | **-1,288 bytes** | +2,572 bytes |
| **ESP8266** | 33,644 (41.1%) | 32,540 (39.7%) | 301,981 (28.9%) | 303,277 (29.0%) | **-1,104 bytes** | +1,296 bytes |

**Analysis:**
- RAM improved on both platforms (~1KB savings from removing static `DebugWS` instance and `_connectedWS` global)
- Flash increased slightly due to new helper functions (`wsPrintf`, `wsPrint`, `wsPrintln`, `wsWrite`)
- Net memory footprint is similar but RAM reduction is beneficial for runtime stability

## Test Results

- [x] ESP32 build passes
- [x] ESP8266 build passes
- [x] Unit tests pass (64 tests)
- [x] Telnet integration tests pass (60/60 tests)
- [x] WebSocket integration tests pass (25/25 tests)

**WebSocket Test Coverage (added 2025-12-10):**
- TC-WS-CON: Connection/handshake tests
- TC-WS-CMD: Command tests (help, memory, levels, toggles)
- TC-WS-FW: Test firmware commands (ping, echo, status)
- TC-WS-STAB: Stability tests (command sequence, long commands)
- TC-WS-API: API method tests (filter, silence, getLastCommand, clearLastCommand, isConnected, callback)
- TC-WS-EDGE: Edge cases (rapid commands, long command, long message, special chars, flood)
