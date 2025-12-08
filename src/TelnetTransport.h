#pragma once

#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#else
#error "Only for ESP8266 or ESP32"
#endif

#include "IDebugTransport.h"
#include "RemoteDebugCfg.h"

class TelnetTransport : public IDebugTransport {
   public:
    TelnetTransport();

    bool begin(uint16_t port) override;
    void stop() override;
    void handle() override;

    bool isConnected() const override;
    void disconnect() override;

    size_t write(const uint8_t* data, size_t len) override;
    int available() override;
    int read(uint8_t* buffer, size_t maxLen) override;

    void setConnectCallback(ConnectCallback cb) override;

    WiFiClient* client();

   private:
    WiFiServer _server;
    WiFiClient _client;
    ConnectCallback _callback = nullptr;
    bool _wasConnected = false;
};
