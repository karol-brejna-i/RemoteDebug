#include "TelnetTransport.h"

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
            WiFiClient newClient = _server.available();
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
            _client = _server.available();
        }

        if (!_client) {
            return;
        }

        _client.setNoDelay(true);
        _client.flush();
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
    return (_client && _client.connected());
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
