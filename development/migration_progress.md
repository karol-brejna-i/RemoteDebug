# RemoteDebug Refactor Migration Progress

## Goals
- Decouple transports (telnet / websocket) from debug logic.
- Isolate command parsing and debug-state management for unit testing.
- Preserve public API and macros while reducing coupling and heap churn.

## Phased Plan
| Step | Description | Status |
| --- | --- | --- |
| Transport seam | Introduce `IDebugTransport` and wrap existing telnet as `TelnetTransport`; keep behavior identical. | Complete |
| Command parser | Extract telnet/WS command handling into a testable parser (no networking dependency). | Complete |
| Output buffering | Move send-buffer logic into a helper so transports share consistent flushing and limits. | Complete |
| Debug core | Encapsulate level/filter/time/profiler flags and line formatting; `RemoteDebug` delegates state queries to it. | Complete |
| WebSocket transport | Implement transport wrapper and remove static WS globals; optional multi-transport support. | Complete |

## Current Status
- Step 1 complete: `IDebugTransport` + `TelnetTransport` wired into `RemoteDebug` without functional change.
- Step 2 complete: `CommandParser` extracts parsing logic; `processCommand()` uses switch on `Command` enum.
- Step 3 complete: `SendBuffer` helper class integrated for `CLIENT_BUFFERING` code paths.
- Step 4 complete: `DebugState` encapsulates all debug configuration; `RemoteDebug` uses `_state.method()` accessors.
- Step 5 complete: `WebSocketTransport` implements `IDebugTransport` for transport parity.
