#pragma once

#include <Arduino.h>

class IDebugTransport {
   public:
    using ConnectCallback = void (*)(bool connected);
    using ReceiveCallback = void (*)(const char* message);
    virtual ~IDebugTransport() = default;

    virtual bool begin(uint16_t port) = 0;
    virtual void stop() = 0;
    virtual void handle() = 0;

    virtual bool isConnected() const = 0;
    virtual void disconnect() = 0;

    virtual size_t write(const uint8_t* data, size_t len) = 0;
    virtual int available() = 0;
    virtual int read(uint8_t* buffer, size_t maxLen) = 0;

    virtual void setConnectCallback(ConnectCallback cb) = 0;
    virtual void setReceiveCallback(ReceiveCallback cb) { (void)cb; }  // Optional, default no-op
};
