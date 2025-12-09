#include "RemoteDebugCfg.h"

#ifndef DEBUG_DISABLED
#if not WEBSOCKET_DISABLED

#include "WebSocketTransport.h"

// Internal debug macro
#define D(fmt, ...)
// #define D(fmt, ...) Serial.printf("wst: " fmt "\n", ##__VA_ARGS__)

// Static instance for callback routing
WebSocketTransport* WebSocketTransport::_instance = nullptr;

WebSocketTransport::WebSocketTransport()
    : _server(nullptr), _clientNum(-1), _connectCallback(nullptr), _hasNewLine(false) {
    _instance = this;
}

WebSocketTransport::~WebSocketTransport() {
    stop();
    _instance = nullptr;
}

bool WebSocketTransport::begin(uint16_t port) {
    if (_server) {
        stop();
    }

    _server = new WebSocketsServer(port);
    if (!_server) {
        return false;
    }

    _server->begin();
    _server->onEvent(onWebSocketEvent);

    D("WebSocket server started on port %u", port);
    return true;
}

void WebSocketTransport::stop() {
    if (_server) {
        _server->close();
        delete _server;
        _server = nullptr;
    }
    _clientNum = -1;
    _receiveBuffer = "";
    _hasNewLine = false;
    D("WebSocket server stopped");
}

void WebSocketTransport::handle() {
    if (_server) {
        _server->loop();
    }
}

bool WebSocketTransport::isConnected() const {
    return (_clientNum >= 0);
}

void WebSocketTransport::disconnect() {
    if (_server && _clientNum >= 0) {
        _server->disconnect(_clientNum);
        _clientNum = -1;

        if (_connectCallback) {
            _connectCallback(false);
        }
    }
    D("disconnect");
}

size_t WebSocketTransport::write(const uint8_t* data, size_t len) {
    if (!_server || _clientNum < 0 || len == 0) {
        return 0;
    }

    // WebSocket sends complete messages, accumulate until newline
    static String sendBuffer;

    for (size_t i = 0; i < len; i++) {
        char c = (char)data[i];
        if (c == '\n') {
            if (sendBuffer.length() > 0) {
                D("write send: %s", sendBuffer.c_str());
                _server->sendTXT(_clientNum, sendBuffer.c_str(), sendBuffer.length());
                sendBuffer = "";
            }
        } else if (c != '\r' && isPrintable(c)) {
            sendBuffer.concat(c);
        }
    }

    return len;
}

int WebSocketTransport::available() {
    return _hasNewLine ? _receiveBuffer.length() : 0;
}

int WebSocketTransport::read(uint8_t* buffer, size_t maxLen) {
    if (!_hasNewLine || _receiveBuffer.length() == 0) {
        return 0;
    }

    size_t len = min(maxLen, (size_t)_receiveBuffer.length());
    memcpy(buffer, _receiveBuffer.c_str(), len);
    _receiveBuffer = "";
    _hasNewLine = false;

    D("read: %u bytes", len);
    return len;
}

void WebSocketTransport::setConnectCallback(ConnectCallback cb) {
    _connectCallback = cb;
}

void WebSocketTransport::sendAppInit() {
    if (_server && _clientNum >= 0) {
        _server->sendTXT(_clientNum, "$app:I");
    }
}

void WebSocketTransport::onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    if (!_instance) return;

    switch (type) {
        case WStype_DISCONNECTED:
            D("[%u] Disconnected", num);
            if (num == _instance->_clientNum) {
                _instance->_clientNum = -1;
                if (_instance->_connectCallback) {
                    _instance->_connectCallback(false);
                }
            }
            break;

        case WStype_CONNECTED:
            D("[%u] Connected", num);
            // Only allow one connection
            if (_instance->_clientNum >= 0 && _instance->_clientNum != num) {
                D("Closing previous connection %d", _instance->_clientNum);
                _instance->_server->sendTXT(_instance->_clientNum, "* Closing client connection ...");
                _instance->_server->disconnect(_instance->_clientNum);
            }

            _instance->_clientNum = num;
            _instance->sendAppInit();

            if (_instance->_connectCallback) {
                _instance->_connectCallback(true);
            }
            break;

        case WStype_TEXT:
            D("[%u] Text: %s", num, payload);
            if (num == _instance->_clientNum && length > 0) {
                _instance->_receiveBuffer = String((char*)payload);
                _instance->_hasNewLine = true;
            }
            break;

        case WStype_ERROR:
            D("Error");
            if (num == _instance->_clientNum) {
                _instance->_clientNum = -1;
                if (_instance->_connectCallback) {
                    _instance->_connectCallback(false);
                }
            }
            break;

        case WStype_PING:
            D("Ping");
            break;

        case WStype_PONG:
            D("Pong");
            break;

        default:
            D("Unhandled WStype %x", type);
            break;
    }
}

#endif  // WEBSOCKET_DISABLED
#endif  // DEBUG_DISABLED
