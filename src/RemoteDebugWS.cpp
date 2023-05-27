///// RemoteDebug configuration]
#include "RemoteDebugCfg.h"

// Debug enabled ?
#ifndef DEBUG_DISABLED

/////// Includes
#include "RemoteDebugWS.h"

// Only if  (RemoteDebugApp) is enabled
#if not WEBSOCKET_DISABLED

#include <WebSockets.h>  // https://github.com/Links2004/arduinoWebSockets
#include <WebSocketsClient.h>
#include <WebSocketsServer.h>

#include "Arduino.h"
#include "RemoteDebug.h"

// Version
#define REMOTEDEBUGWS_VERSION "0.1.1"

// Internal debug macro - recommended stay disable
#define D(fmt, ...)
// use the following line to enable debug
// #define D(fmt, ...) Serial.printf("rdws: " fmt "\n", ##__VA_ARGS__);  // Serial debug

// websocket server
static WebSocketsServer WebSocketServer(WEBSOCKET_PORT);  // Websocket server on port 81

// Variables used by webSocketEvent
static int8_t _webSocketConnected = WS_NOT_CONNECTED;  // Client is connected ?
static RemoteDebugWSCallbacks* _callbacks;             // callbacks to RemoteDebug

// Initialize the socket server
void RemoteDebugWS::begin(RemoteDebugWSCallbacks* callbacks) {
    // Initialize WebSockets
    WebSocketServer.begin();                  // start the websocket server
    WebSocketServer.onEvent(webSocketEvent);  // if there's an incomming websocket message, go to function 'webSocketEvent'

    // Set callbacks
    _callbacks = callbacks;

    // Debug
    D("socket server started")
}

// Finalize the socket server
void RemoteDebugWS::stop() {
    // Finalize  (RemoteDebugApp)

    WebSocketServer.close();
    _webSocketConnected = WS_NOT_CONNECTED;
    D("socket server stopped")
}

void RemoteDebugWS::disconnectAllClients() {
    // Disconnect all clients
    WebSocketServer.disconnect();

    // Disconnected
    _webSocketConnected = WS_NOT_CONNECTED;

    // Callback
    if (_callbacks) {
        _callbacks->onDisconnect();
    }
    D("disconnectAllClients")
}

void RemoteDebugWS::disconnect() {
    D("disconnect")
    // Disconnect actual clients
    if (_webSocketConnected != WS_NOT_CONNECTED) {
        WebSocketServer.disconnect(_webSocketConnected);

        // Disconnected
        _webSocketConnected = WS_NOT_CONNECTED;

        // Callback
        if (_callbacks) {
            _callbacks->onDisconnect();
        }
    }
}

// Handle
void RemoteDebugWS::handle() {
    WebSocketServer.loop();
}

// Is connected ?
boolean RemoteDebugWS::isConnected() {
    return (_webSocketConnected != WS_NOT_CONNECTED);
}

// Print
size_t RemoteDebugWS::write(const uint8_t* buffer, size_t size) {
    if (size != 0) {
        D("write: %u characters", size)
    } else {
        return 0;
    }

    for (size_t i = 0; i < size; i++) {
        write((uint8_t)buffer[i]);
    }

    return size;
}

size_t RemoteDebugWS::write(uint8_t character) {
    static String buffer = "";
    size_t ret = 0;

    // Process
    if (character == '\n') {
        if (_webSocketConnected != WS_NOT_CONNECTED) {
            D("write send buf[%u]: %s", buffer.length(), buffer.c_str());
            WebSocketServer.sendTXT(_webSocketConnected, buffer.c_str(), buffer.length());
        }

        // Empty the buffer
        ret = buffer.length();
        buffer = "";
    } else if (character != '\r' && isPrintable(character)) {
        buffer.concat((char)character);
    }

    return ret;
}

// Destructor
RemoteDebugWS::~RemoteDebugWS() {
    stop();
}

/////// WebSocket to comunicate with RemoteDebugApp
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t payloadlength) {  // When a WebSocket message is received

    IPAddress localip;

    switch (type) {
        case WStype_DISCONNECTED:  // if the websocket is disconnected

            D("[%u] Disconnected!", num);

            // Disconnected
            _webSocketConnected = WS_NOT_CONNECTED;

            // Callback
            if (_callbacks) {
                _callbacks->onDisconnect();
            }

            break;
        case WStype_CONNECTED:  // if a new websocket connection is established
        {
#ifdef D
            IPAddress ip = WebSocketServer.remoteIP(num);
            D("[%u] Connected from %d.%d.%d.%d url: %s", num, ip[0], ip[1], ip[2], ip[3], payload);
#endif

            // Have in another connection
            // One connection to reduce overheads
            if (num != _webSocketConnected) {
                D("Another connection, closing ...")
                WebSocketServer.sendTXT(_webSocketConnected, "* Closing client connection ...");
                WebSocketServer.disconnect(_webSocketConnected);
            }

            // Set as connected
            _webSocketConnected = num;

            // Send initial message
            WebSocketServer.sendTXT(_webSocketConnected, "$app:I");

            // Callback
            if (_callbacks) {
                _callbacks->onConnect();
            }
        } break;

        case WStype_TEXT:  // if new text data is received
        {
            // if (payloadlength == 0) {
            //	return;
            // }

            D("[%u] get Text: %s", num, payload);

            // Callback
            if (_callbacks) {
                _callbacks->onReceive((const char*)payload);
            }

        } break;

        case WStype_ERROR:  // if new text data is received
            D("Error")
            // Disconnected
            _webSocketConnected = WS_NOT_CONNECTED;

            // Callback
            if (_callbacks) {
                _callbacks->onDisconnect();
            }

            break;
        case WStype_PONG:
            D("Pong")
            break;
        case WStype_PING:
            D("Ping")
            break;
        default:
            D("WStype %x not handled ", type)
            // Disconnected
            _webSocketConnected = WS_NOT_CONNECTED;

            // Callback
            if (_callbacks) {
                _callbacks->onDisconnect();
            }
    }
}

#endif  // WEBSOCKET_DISABLED
#endif  // DEBUG_DISABLED
