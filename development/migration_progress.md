# RemoteDebug Refactor Migration Progress

## Goals
- Decouple transports (telnet / websocket) from debug logic.
- Isolate command parsing and debug-state management for unit testing.
- Preserve public API and macros while reducing coupling and heap churn.

## Phased Plan
| Step | Description | Status |
| --- | --- | --- |
| Transport seam | Introduce `IDebugTransport` and wrap existing telnet as `TelnetTransport`; keep behavior identical. | Complete |
| Command parser | Extract telnet/WS command handling into a testable parser (no networking dependency). | Not started |
| Debug core | Encapsulate level/filter/time/profiler flags and line formatting; `RemoteDebug` delegates state queries to it. | Not started |
| Output buffering | Move send-buffer logic into a helper so transports share consistent flushing and limits. | Not started |
| WebSocket transport | Implement transport wrapper and remove static WS globals; optional multi-transport support. | Not started |

## Current Status
- Plan written (this file).
- Step 1 complete: `IDebugTransport` + `TelnetTransport` wired into `RemoteDebug` without functional change.
