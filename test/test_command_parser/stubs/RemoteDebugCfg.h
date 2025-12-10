/**
 * Minimal RemoteDebugCfg.h stub for native unit testing
 */

#pragma once

#define VERSION "4.1.1"

// Essential defines for compilation
#ifndef DEBUG_DISABLED
// Keep debug enabled for tests
#endif

#define TELNET_PORT 23
#define WEBSOCKET_PORT 81
#define MAX_TIME_INACTIVE 0
#define BUFFER_PRINT 150
#define CLIENT_BUFFERING true
#define DELAY_TO_SEND 10
#define MAX_SIZE_SEND 1460
