#pragma once

#include "RemoteDebugCfg.h"

#ifndef DEBUG_DISABLED
#if not WEBSOCKET_DISABLED

#include "IDebugTransport.h"

#include <WebSockets.h>
#include <WebSocketsServer.h>

/**
 * WebSocket transport implementing IDebugTransport interface.
 * Wraps WebSocketsServer to provide transport abstraction parity with TelnetTransport.
 */
class WebSocketTransport : public IDebugTransport {
   public:
    WebSocketTransport();
    ~WebSocketTransport() override;

    // IDebugTransport interface
    bool begin(uint16_t port) override;
    void stop() override;
    void handle() override;

    bool isConnected() const override;
    void disconnect() override;

    size_t write(const uint8_t* data, size_t len) override;
    int available() override;
    int read(uint8_t* buffer, size_t maxLen) override;

    void setConnectCallback(ConnectCallback cb) override;

    // WebSocket-specific: send initial app message
    void sendAppInit();

   private:
    static void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

    static WebSocketTransport* _instance;  // For static callback routing

    WebSocketsServer* _server;
    int8_t _clientNum;
    ConnectCallback _connectCallback;
    String _receiveBuffer;
    bool _hasNewLine;
};

#endif  // WEBSOCKET_DISABLED
#endif  // DEBUG_DISABLED
