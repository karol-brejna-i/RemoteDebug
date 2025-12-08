#include "TelnetTransport.h"

namespace {

// ESP32 renamed WiFiServer::available() to accept(); keep both without warnings.
static inline WiFiClient acceptClient(WiFiServer& server) {
#if defined(ESP32)
    return server.accept();
#else
    return server.available();
#endif
}

// NetworkClient::clear() replaces flush() on newer cores; guard for compatibility.
static inline void clearClientBuffer(WiFiClient& client) {
#if defined(ESP32)
    client.clear();
#else
    client.flush();
#endif
}

}  // namespace

TelnetTransport::TelnetTransport() : _server(TELNET_PORT) {}

bool TelnetTransport::begin(uint16_t port) {
    if (port != TELNET_PORT) {
        return false;  // existing behavior: only default port supported
    }

    _server = WiFiServer(port);
    _server.begin();
    _server.setNoDelay(true);
    return true;
}

void TelnetTransport::stop() {
    if (_client && _client.connected()) {
        _client.stop();
    }
    _server.stop();
    _wasConnected = false;
}

void TelnetTransport::handle() {
    if (_server.hasClient()) {
        if (_client && _client.connected()) {
            WiFiClient newClient = acceptClient(_server);
            if (newClient && newClient.remoteIP() == _client.remoteIP()) {
                _client.stop();
                _client = newClient;
            } else {
                if (newClient) {
                    newClient.stop();
                }
                return;
            }
        } else {
            _client = acceptClient(_server);
        }

        if (!_client) {
            return;
        }

        _client.setNoDelay(true);
        clearClientBuffer(_client);
        delay(CONNECTION_BUFFER_CLEAR_DELAY_MS);
        while (_client.available()) {
            _client.read();
        }

        _wasConnected = true;
        if (_callback) {
            _callback(true);
        }
    }

    bool nowConnected = (_client && _client.connected());
    if (!nowConnected && _wasConnected) {
        if (_callback) {
            _callback(false);
        }
        _wasConnected = false;
    }
}

bool TelnetTransport::isConnected() const {
    return _client.connected();
}

void TelnetTransport::disconnect() {
    if (_client && _client.connected()) {
        _client.stop();
    }
    if (_wasConnected && _callback) {
        _callback(false);
    }
    _wasConnected = false;
}

size_t TelnetTransport::write(const uint8_t* data, size_t len) {
    if (!isConnected() || len == 0) {
        return 0;
    }
    return _client.write(data, len);
}

int TelnetTransport::available() {
    if (!isConnected()) {
        return 0;
    }
    return _client.available();
}

int TelnetTransport::read(uint8_t* buffer, size_t maxLen) {
    if (!isConnected() || maxLen == 0) {
        return 0;
    }
    return _client.read(buffer, maxLen);
}

void TelnetTransport::setConnectCallback(ConnectCallback cb) {
    _callback = cb;
}

WiFiClient* TelnetTransport::client() {
    return &_client;
}
