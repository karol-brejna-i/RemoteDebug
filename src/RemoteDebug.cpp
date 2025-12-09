///// RemoteDebug configuration
#include "RemoteDebugCfg.h"

///// Debug disable for compile to production/release ?
///// as nothing of RemotedDebug is compiled, zero overhead :-)
#ifndef DEBUG_DISABLED

///// Includes
#include "stdint.h"

#if defined(ESP8266)
// ESP8266 SDK
extern "C" {
bool system_update_cpu_freq(uint8_t freq);
}
#endif  // ESP8266

#include "Arduino.h"
#include "Print.h"

#ifdef SERIAL_DEBUG_H
// Cannot used with SerialDebug at same time
#error "RemoteDebug cannot be used with SerialDebug"
#endif

// ESP8266 or ESP32 ?
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#else
#error Only for ESP8266 or ESP32
#endif  // ESP8266 or ESP32 ?

#include "RemoteDebug.h"  // This library

#ifdef ALPHA_VERSION  // In test, not good yet
#include "telnet.h"
#endif

// Support to websocket connection with RemoteDebugApp
// Note: you must install the arduinoWebSocket library before

// Internal debug macro - recommended to stay disabled
#define D(fmt, ...)
// use the following line to enable debug
// #define D(fmt, ...) Serial.printf("rd: " fmt "\n", ##__VA_ARGS__);  // Serial debug

// Forward declarations for WS helper functions
#if not WEBSOCKET_DISABLED
static void wsPrintf(const char* fmt, ...);
static void wsPrint(const String& str);
static void wsPrintln(const String& str);
#endif

// Internal print macros for send messages to client

#if not WEBSOCKET_DISABLED

#define debugPrintf(fmt, ...)                                                        \
    {                                                                                \
        WiFiClient* client = (_instance ? _instance->getTelnetClient() : nullptr);   \
        if (_connected && client)                                                    \
            client->printf(fmt, ##__VA_ARGS__);                                      \
        else if (_instance && _instance->isWsConnected())                            \
            wsPrintf(fmt, ##__VA_ARGS__);                                            \
    }
#define debugPrintln(str)                                                            \
    {                                                                                \
        WiFiClient* client = (_instance ? _instance->getTelnetClient() : nullptr);   \
        if (_connected && client)                                                    \
            client->println(str);                                                    \
        else if (_instance && _instance->isWsConnected())                            \
            wsPrintln(str);                                                          \
    }
#define debugPrint(str)                                                              \
    {                                                                                \
        WiFiClient* client = (_instance ? _instance->getTelnetClient() : nullptr);   \
        if (_connected && client)                                                    \
            client->print(str);                                                      \
        else if (_instance && _instance->isWsConnected())                            \
            wsPrint(str);                                                            \
    }

#else  // Websocket disabled

#define debugPrintf(fmt, ...)                                                        \
    {                                                                                \
        WiFiClient* client = (_instance ? _instance->getTelnetClient() : nullptr);   \
        if (_connected && client) client->printf(fmt, ##__VA_ARGS__);                \
    }
#define debugPrintln(str)                                                            \
    {                                                                                \
        WiFiClient* client = (_instance ? _instance->getTelnetClient() : nullptr);   \
        if (_connected && client) client->println(str);                              \
    }
#define debugPrint(str)                                                              \
    {                                                                                \
        WiFiClient* client = (_instance ? _instance->getTelnetClient() : nullptr);   \
        if (_connected && client) client->print(str);                                \
    }

#endif

// Static lookup tables for debug level prefixes and colors (avoid switch/case overhead)
static const char* const DEBUG_LEVEL_PREFIXES[] = {
    "(P",  // PROFILER = 0
    "(V",  // VERBOSE = 1
    "(D",  // DEBUG = 2
    "(I",  // INFO = 3
    "(W",  // WARNING = 4
    "(E"   // ERROR = 5
};

static const char* const DEBUG_LEVEL_COLORS[] = {
    "",               // PROFILER = 0 (no special color)
    COLOR_VERBOSE,    // VERBOSE = 1
    COLOR_DEBUG,      // DEBUG = 2
    COLOR_INFO,       // INFO = 3
    COLOR_WARNING,    // WARNING = 4
    COLOR_ERROR       // ERROR = 5
};

// Instance
static RemoteDebug* _instance;

// WebSocket helper functions for printf-style output
#if not WEBSOCKET_DISABLED

static void wsPrintf(const char* fmt, ...) {
    if (!_instance || !_instance->isWsConnected()) return;
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    String s(buf);
    s.concat('\n');  // WebSocket messages are newline-terminated
    _instance->wsWrite(s);
}

static void wsPrint(const String& str) {
    if (!_instance || !_instance->isWsConnected()) return;
    _instance->wsWrite(str);
}

static void wsPrintln(const String& str) {
    if (!_instance || !_instance->isWsConnected()) return;
    String s = str;
    s.concat('\n');
    _instance->wsWrite(s);
}

#endif  // WEBSOCKET_DISABLED

////// Methods / routines

// Constructor

RemoteDebug::RemoteDebug() {
    D("RemoteDebug constructor")
    // Save the instance
    _instance = this;
}

// Initialize the telnet server

bool RemoteDebug::begin(String hostName, uint8_t startingDebugLevel) {
    return begin(hostName, TELNET_PORT, startingDebugLevel);
}

bool RemoteDebug::begin(String hostName, uint16_t port, uint8_t startingDebugLevel) {
    D("RemoteDebug begin")
    // Initialize server telnet
    _telnetTransport.setConnectCallback([](bool connected) {
        if (_instance) {
            _instance->_connected = connected;
            if (connected) {
                _instance->onConnection(true);
            }
        }
    });

    if (!_telnetTransport.begin(port)) {  // Bug: not more can use begin(port)..
        return false;
    }

    D("WEB_SOCKETS_DISABLED: %d", WEBSOCKET_DISABLED)
#if not WEBSOCKET_DISABLED
    // Initialize WebSocket transport (for RemoteDebugApp)
    _wsTransport.setConnectCallback([](bool connected) {
        if (_instance) {
            if (connected) {
                D("rd: WS onconnect")
                // Is telnet connected -> disconnect it to reduce overhead
                if (_instance->isConnected()) {
                    D("disconnect telnet, because WS is connected")
                    _instance->disconnect(true);
                }
                _instance->onConnection(true);
            } else {
                D("rd: WS ondisconnect")
            }
        }
    });
    _wsTransport.setReceiveCallback([](const char* message) {
        if (_instance) {
            D("rd: WS onreceive")
            _instance->wsOnReceive(message);
        }
    });
    _wsTransport.begin(WEBSOCKET_PORT);
#endif

    // Reserve space to buffer of print writes

    _bufferPrint.reserve(BUFFER_PRINT);

    // Reserve space for command buffers
    _command.reserve(64);
    _lastCommand.reserve(64);

#ifdef CLIENT_BUFFERING
    // Reserve space to buffer of send
    _sendBuffer.reserve(MAX_SIZE_SEND);

#endif

    // Host name of this device

    _hostName = hostName;

    // Debug level

    _state.setLevel(startingDebugLevel);
    _state.setLastLevel(startingDebugLevel);

    return true;
}

#ifdef DEBUGGER_ENABLED
// Simple software debugger - based on SerialDebug Library
void RemoteDebug::initDebugger(boolean (*callbackEnabled)(), void (*callbackHandle)(const boolean), String (*callbackGetHelp)(), void (*callbackProcessCmd)()) {
    // Init callbacks for the debugger

    _callbackDbgEnabled = callbackEnabled;
    _callbackDbgHandle = callbackHandle;
    _callbackDbgHelp = callbackGetHelp;
    _callbackDbgProcessCmd = callbackProcessCmd;
}

WiFiClient* RemoteDebug::getTelnetClient() {
    return _telnetTransport.client();
}

#endif

// Set the password for telnet - thanks @jeroenst for suggesting this method

void RemoteDebug::setPassword(String password) {
    _password = password;
}

// Destructor

RemoteDebug::~RemoteDebug() {
    // Flush
    WiFiClient* client = getTelnetClient();
    if (client && client->connected()) {
    // Avoid deprecated flush() where clear() exists
#if defined(ESP32)
    client->clear();
#else
    client->flush();
#endif
    }

    // Stop
    stop();
}

// Stop the server

void RemoteDebug::stop() {
    D("rd: stop")
    // Stop Client
    _telnetTransport.stop();

#if not WEBSOCKET_DISABLED
    // Stop WebSocket server (RemoteDebugApp)
    _wsTransport.stop();
#endif
}

// Handle the connection (in begin of loop in sketch)
// TODO: optimize when loop not have a large delay
void RemoteDebug::handle() {
#ifdef ALPHA_VERSION  // In test, not good yet
    static uint32_t lastTime = millis();
#endif

#ifdef DEBUGGER_ENABLED
    static uint32_t dbgTimeHandle = millis();  // To avoid calling the handler unnecessarily
    static boolean dbgLastConnected = false;   // Last is connected ?
#endif

    // Silence timeout ?

    if (_state.isSilence() && _state.silenceTimeout() > 0 && millis() >= _state.silenceTimeout()) {
        // Get out of silence mode
        silence(false, true);
    }

    // Debug level is profiler -> set the level before
    if (_state.getLevel() == PROFILER) {
        if (millis() > _state.getLevelProfilerDisable()) {
            _state.setLevel(_state.getLevelBeforeProfiler());
            debugPrintln("* Debug level profile inactive now");
        }
    }

#ifdef ALPHA_VERSION  // In test, not good yet

    // Automatic change to profiler level if time between handles is greater than n millis
    if (_state.getAutoLevelProfiler() > 0 && _state.getLevel() != PROFILER) {
        uint32_t diff = (millis() - lastTime);

        if (diff >= _state.getAutoLevelProfiler()) {
            _state.setLevelBeforeProfiler(_state.getLevel());
            _state.setLevel(PROFILER);
            _state.setLevelProfilerDisable(PROFILER_DEFAULT_TIMEOUT_MS);  // Disable after default timeout

            debugPrintf("* Debug level profile active now - time between handels: %u\r\n", diff);
        }

        lastTime = millis();
    }
#endif

    // Transport handling (accepts new clients)
    _telnetTransport.handle();

    // Is client connected ? (to reduce overhead in active)
    _connected = _telnetTransport.isConnected();

    // Get command over telnet
    if (_connected) {
        char last = ' ';  // To avoid process two times the "\r\n"

        while (_telnetTransport.available()) {  // get data from Client

            // Get character
            char character;
            if (_telnetTransport.read((uint8_t*)&character, 1) != 1) {
                break;
            }

            // Newline (CR or LF) - once one time if (\r\n) - 26/07/17
            if (isCRLF(character) == true) {
                if (isCRLF(last) == false) {
                    // Process the command

                    if (_command.length() > 0) {
                        _lastCommand = _command;  // Store the last command
                        processCommand();
                    }
                }

                _command = "";  // Init it for next command

            } else if (isPrintable(character)) {
                // Concat
                _command.concat(character);
            }

            // Last char
            last = character;
        }
    }

    // Client connected ?

#if not WEBSOCKET_DISABLED
    boolean connected = (_connected || _wsTransport.isConnected());
#else  // By telnet
    boolean connected = _connected;
#endif

    if (connected) {
#ifdef CLIENT_BUFFERING
        // Client buffering - send data in intervals to avoid delays or if its is too big

        _sendBuffer.tick(millis(), [this](const String& payload) { debugPrint(payload); });
#endif

#ifdef MAX_TIME_INACTIVE
#if MAX_TIME_INACTIVE > 0

        // Inactivity - close connection if not received commands from user in telnet
        // For reduce overheads

        uint32_t maxTime = MAX_TIME_INACTIVE;  // Normal

        if (_password != "" && !_passwordOk) {  // Request password - 18/08/08
            maxTime = PASSWORD_TIMEOUT_MS;      // Timeout for password entry
        } else
            maxTime = connectionTimeout;  // When password is ok set normal timeout

        if ((maxTime > 0) && ((millis() - _lastTimeCommand) > maxTime)) {
            debugPrintln("* Closing session by inactivity");

            // Disconnect

            disconnect();
            return;
        }
#endif
#endif
    }

#if not WEBSOCKET_DISABLED  // For websocket server
    _wsTransport.handle();
#endif

#ifdef DEBUGGER_ENABLED
    // For Simple software debugger - based on SerialDebug Library
    // Changed handle debugger logic - 2018-03-01
    if (_callbackDbgEnabled && _callbackDbgHandle) {  // Calbacks ok ?

        boolean callHandle = false;

        if (dbgLastConnected != connected) {  // Change connection -> always call
            dbgLastConnected = connected;
            callHandle = true;
        } else if (millis() >= dbgTimeHandle) {
            if (_callbackDbgEnabled()) {  // Only if it is enabled
                callHandle = true;
            }
        }

        if (callHandle) {
            // Call the handle
            _callbackDbgHandle(true);
            // Save time
            dbgTimeHandle = millis() + DEBUGGER_HANDLE_TIME;
        }
    }
#endif

    // DV("*handle time: ", (millis() - timeBegin));
}

// Disconnect client

void RemoteDebug::disconnect(boolean onlyTelnetClient) {
    // Disconnect
    // TODO XXX show info about onlyTelnetClient
    D("rd onlyTelnetClient %d", onlyTelnetClient);
    if (onlyTelnetClient) {
        if (_connected) {
            WiFiClient* client = getTelnetClient();
            if (client) {
                client->println("* Closing client connection ...");  // this is to web app new conn not receive it
            }
        }
    } else {
        debugPrintln("* Closing client connection ...");
    }

    _state.setSilence(false);
    _state.setSilenceTimeout(0);

    if (_connected) {  // By telnet
        _telnetTransport.disconnect();
        _connected = false;
    }
#if not WEBSOCKET_DISABLED
    if (_wsTransport.isConnected() && !onlyTelnetClient) {
        D("rd _wsTransport.disconnect()")
        _wsTransport.disconnect();
    }
#endif
}

// Connection/disconnection event

void RemoteDebug::onConnection(boolean connected) {
    // Clear variables

    D("rd onconn %d", connected);

    _bufferPrint = "";            // Clean buffer
    _lastTimeCommand = millis();  // To mark time for inactivity
    _command = "";                // Clear command
    _lastCommand = "";            // Clear las command
    _state.setLastTimePrint(millis());    // Clear the time
    _state.setSilence(false);             // No silence
    _state.setSilenceTimeout(0);

#ifdef CLIENT_BUFFERING
    // Client buffering - send data in intervals to avoid delays or if its is too big
    _sendBuffer.reset(millis());
#endif

    // Password request ? - 18/07/18

    if (_password != "") {
        _passwordOk = false;

#ifdef REMOTEDEBUG_PWD_ATTEMPTS
        _passwordAttempt = 1;
#endif
    }

    // Save it
    _connected = connected;

    // Process
    if (connected) {  // Connected ?

        // Callback

        if (_callbackNewClient) {
            _callbackNewClient();
        }

        // Show the initial message
#if SHOW_HELP
        showHelp();
#endif
    }
}

boolean RemoteDebug::isConnected() {
    // Is connected

#if not WEBSOCKET_DISABLED
    return (_connected || _wsTransport.isConnected());
#else
    return _connected;
#endif
}

// Send to serial too (use only if need)
void RemoteDebug::setSerialEnabled(boolean enable) {
    _state.setSerialEnabled(enable);
    _state.setShowColors(false);  // Disable it for Serial
}

// Allow ESP reset over telnet client

void RemoteDebug::setResetCmdEnabled(boolean enable) {
    _resetCommandEnabled = enable;
}

// Show time in millis

void RemoteDebug::showTime(boolean show) {
    _state.setShowTime(show);
}

// Show profiler - time in millis between messages of debug

void RemoteDebug::showProfiler(boolean show, uint32_t minTime) {
    _state.setShowProfiler(show);
    _state.setMinTimeShowProfiler(minTime);
}

#ifdef ALPHA_VERSION  // In test, not good yet
// Automatic change to profiler level if time between handles is greater than n mills (0 - disable)

void RemoteDebug::autoProfilerLevel(uint32_t millisElapsed) {
    _state.setAutoLevelProfiler(millisElapsed);
}
#endif

// Show debug level

void RemoteDebug::showDebugLevel(boolean show) {
    _state.setShowDebugLevel(show);
}

// Show colors

void RemoteDebug::showColors(boolean show) {
    if (_state.serialEnabled() == false) {
        _state.setShowColors(show);
    } else {
        _state.setShowColors(false);  // Disable it for Serial
    }
}

// Show in raw mode - only data ?

void RemoteDebug::showRaw(boolean show) {
    _state.setShowRaw(show);
}

// Is active ? client telnet connected and level of debug equal or greater then set by user in telnet

boolean RemoteDebug::isActive(uint8_t debugLevel) {
    // Active ->
    //	Not in silence (new)
    //	Debug level ok and
    //	Telnet connected or
    //	Serial enabled (use only if need)
    //	Password ok (if enabled) - 18/08/18

#if not WEBSOCKET_DISABLED
    boolean ret = (debugLevel >= _state.getLevel() &&
                   !_state.isSilence() &&
                   (_connected || _wsTransport.isConnected() || _state.serialEnabled()));
#else  // Telnet only
    boolean ret = (debugLevel >= _state.getLevel() &&
                   !_state.isSilence() &&
                   (_connected || _state.serialEnabled()));
#endif
    if (ret) {
        _state.setLastLevel(debugLevel);
    }

    return ret;
}

// Set help for commands over telnet set by sketch
void RemoteDebug::setHelpProjectsCmds(String help) {
    _helpProjectCmds = help;
}

// Set callback of sketch function to process project messages
void RemoteDebug::setCallBackProjectCmds(void (*callback)()) {
    _callbackProjectCmds = callback;
}

void RemoteDebug::setCallBackNewClient(void (*callback)()) {
    _callbackNewClient = callback;
}

// Print
size_t RemoteDebug::write(const uint8_t* buffer, size_t size) {
    // Process buffer
    // Insert due a write bug w/ latest Esp8266 SDK - 17/08/18

    for (size_t i = 0; i < size; i++) {
        write((uint8_t)buffer[i]);
    }

    return size;
}

size_t RemoteDebug::write(uint8_t character) {
    // Write logic
    uint32_t elapsed = 0;
    size_t ret = 0;
    String colorLevel = "";

    // Connected ?
#if not WEBSOCKET_DISABLED
    boolean connected = (_connected || _wsTransport.isConnected());
#else
    boolean connected = _connected;
#endif

    // In silent mode now ?
    if (_state.isSilence()) {
        return 0;
    }

    // New line writted before ?
    if (_state.isNewLine()) {
#ifdef DEBUGGER_ENABLED
        // For Simple software debugger - based on SerialDebug Library
        // Changed handle debugger logic - 2018-02-29
        if (!_state.showRaw()) {  // Not for raw mode

            if (_callbackDbgEnabled && _callbackDbgEnabled()) {  // Callbacks ok

                if (connected && _callbackDbgEnabled()) {  // Only call if connected and debugger is enabled
                    // Call the handle
                    _callbackDbgHandle(false);
                }
            }
        }
#endif

        String show = "";
        show.reserve(80);  // Pre-allocate: color(5) + level(2) + time(15) + profiler(25) + separator

        // Not in raw mode (only data)
        if (!_state.showRaw()) {
            uint8_t level = _state.getLastLevel();
            
            // New color system - use lookup table
            if (_state.showColors() && level >= VERBOSE && level <= ERROR) {
                show = DEBUG_LEVEL_COLORS[level];
                colorLevel = show;
            }

            // Show debug level - use lookup table
            if (_state.showDebugLevel() && level <= ERROR) {
                show.concat(DEBUG_LEVEL_PREFIXES[level]);
            }

            // Show time in millis - use snprintf instead of multiple concat
            if (_state.showTime()) {
                char timeBuf[20];
                snprintf(timeBuf, sizeof(timeBuf), "%st:%lums", 
                         (show.length() > 0 ? " " : ""), millis());
                show.concat(timeBuf);
            }

            // Show profiler (time between messages)
            if (_state.showProfiler()) {
                elapsed = (millis() - _state.lastTimePrint());
                boolean resetColors = false;
                if (show.length() > 0) {
                    show.concat(" ");
                }
                if (_state.showColors()) {
                    if (elapsed < PROFILER_THRESHOLD_GREEN_MS) {
                        ;  // not color this
                    } else if (elapsed < PROFILER_THRESHOLD_YELLOW_MS) {
                        show.concat(COLOR_BLACK COLOR_BACKGROUND_GREEN);
                        resetColors = true;
                    } else if (elapsed < PROFILER_THRESHOLD_MAGENTA_MS) {
                        show.concat(COLOR_BLACK COLOR_BACKGROUND_YELLOW);
                        resetColors = true;
                    } else if (elapsed < PROFILER_THRESHOLD_RED_MS) {
                        show.concat(COLOR_WHITE COLOR_BACKGROUND_MAGENTA);
                        resetColors = true;
                    } else {
                        show.concat(COLOR_WHITE COLOR_BACKGROUND_RED);
                        resetColors = true;
                    }
                }
                // Use snprintf for profiler formatting instead of formatNumber + multiple concat
                char profBuf[20];
                snprintf(profBuf, sizeof(profBuf), "p:^%4lums", elapsed);
                show.concat(profBuf);
                if (resetColors) {
                    show.concat(COLOR_RESET);
                    show.concat(colorLevel);
                }
                _state.setLastTimePrint(millis());
            }
        } else {  // Raw mode - only data - e.g. used for debugger messages
            show.concat(COLOR_RAW);
        }

        // Show anything ?
        if (show.length() > 0) {
            if (!_state.showRaw()) {
                show.concat(") ");
            }

            // Write to telnet buffered
            if (connected || _state.serialEnabled()) {  // send data to Client
                _bufferPrint = show;
            }
        }

        _state.setNewLine(false);
    }

    // Print ?
    boolean doPrint = false;

    // New line ?
    if (character == '\n') {
        _bufferPrint.concat("\r");  // Para clientes windows - 29/01/17

        _state.setNewLine(true);
        doPrint = true;

    } else if (_bufferPrint.length() == BUFFER_PRINT) {  // Limit of buffer
        doPrint = true;
    }

    // Write to telnet Buffered
    _bufferPrint.concat((char)character);

    // Send the characters buffered by print.h
    if (doPrint) {  // Print the buffer
        boolean noPrint = false;

        if (_state.showProfiler() && elapsed < _state.minTimeShowProfiler()) {  // Profiler time Minimal
            noPrint = true;
        } else if (_state.filterActive()) {  // Check filter before print

            String aux = _bufferPrint;
            aux.toLowerCase();

            if (aux.indexOf(_state.filter()) == -1) {  // not find -> no print
                noPrint = true;
            }
        }

        if (noPrint == false) {
            if (_state.showColors()) _bufferPrint.concat(COLOR_RESET);
            // Send to telnet or websocket (buffered)
            boolean sendToClient = connected;

            if (_password != "" && !_passwordOk) {  // With no password -> no telnet output - 2018-10-19
                sendToClient = false;
            }

            if (sendToClient) {  // send data to Client

#ifndef CLIENT_BUFFERING
                debugPrint(_bufferPrint);
#else  // Client buffering
                _sendBuffer.enqueue(_bufferPrint, millis(), _state.showRaw(),
                                    [this](const String& payload) { debugPrint(payload); });
#endif
            }

            // Echo to serial (not buffering it)
            if (_state.serialEnabled()) {
                Serial.print(_bufferPrint);
            }
        }

        // Empty the buffer
        ret = _bufferPrint.length();
        _bufferPrint = "";
    }
    return ret;
}

/**
 * @brief Show help of commands
 *
 */
void RemoteDebug::showHelp() {
    D("showHelp")
    String help = "";
    help.reserve(2048);  // Pre-allocate for help text (measured ~1.5KB typical)

    // Password request ? - 04/03/18
    if (_password != "" && !_passwordOk) {
        help.concat("\r\n");
        help.concat("* Please enter with a password to access");
#ifdef REMOTEDEBUG_PWD_ATTEMPTS
        help.concat(" (attempt ");
        help.concat(_passwordAttempt);
        help.concat(" of ");
        help.concat(REMOTEDEBUG_PWD_ATTEMPTS);
        help.concat(")");
#endif
        help.concat(':');
        help.concat("\r\n");

        debugPrint(help);

        return;
    }

    // Show help

#if defined(ESP8266)
    help.concat("*** Remote debug - over telnet - for ESP8266 (NodeMCU) - version ");
#elif defined(ESP32)
    help.concat("*** Remote debug - over telnet - for ESP32 - version ");
#endif
    help.concat(VERSION);
    help.concat("\r\n");
    help.concat("* Host name: ");
    help.concat(_hostName);
    help.concat(" IP:");
    help.concat(WiFi.localIP().toString());
    help.concat(" Mac address:");
    help.concat(WiFi.macAddress());
    help.concat("\r\n");
    help.concat("* Free Heap RAM: ");
    help.concat(ESP.getFreeHeap());
    help.concat("\r\n");
    help.concat("* ESP SDK version: ");
    help.concat(ESP.getSdkVersion());
    help.concat("\r\n");
    help.concat("******************************************************\r\n");
    help.concat("* Commands:\r\n");
    help.concat("    ? or help -> display these help of commands\r\n");
    help.concat("    q -> quit (close this connection)\r\n");
    help.concat("    m -> display memory available\r\n");
    help.concat("    v -> set debug level to verbose\r\n");
    help.concat("    d -> set debug level to debug\r\n");
    help.concat("    i -> set debug level to info\r\n");
    help.concat("    w -> set debug level to warning\r\n");
    help.concat("    e -> set debug level to errors\r\n");
    help.concat("    s -> set debug silence on/off\r\n");
    help.concat("    l -> show debug level\r\n");
    help.concat("    t -> show time (millis)\r\n");
    help.concat("    timeout -> set connection timeout (sec, 0 = disabled)\r\n");
    help.concat("    profiler:\r\n");
    help.concat("      p      -> show time between actual and last message (in millis)\r\n");
    help.concat("      p min  -> show only if time is this minimal\r\n");
    help.concat("      P time -> set debug level to profiler\r\n");
#ifdef ALPHA_VERSION  // In test, not good yet
    help.concat("      A time -> set auto debug level to profiler\r\n");
#endif
    help.concat("    c -> show colors\r\n");
    help.concat("    filter:\r\n");
    help.concat("          filter <string> -> show only debugs with this\r\n");
    help.concat("          nofilter        -> disable the filter\r\n");
#if defined(ESP8266)
    help.concat("    cpu80  -> ESP8266 CPU a 80MHz\r\n");
    help.concat("    cpu160 -> ESP8266 CPU a 160MHz\r\n");
    if (_resetCommandEnabled) {
        help.concat("    reset -> reset the ESP8266\r\n");
    }
#elif defined(ESP32)
    if (_resetCommandEnabled) {
        help.concat("    reset -> reset the ESP32\r\n");
    }
#endif

    // Callbacks
    if (_helpProjectCmds != "" && (_callbackProjectCmds)) {
        help.concat("\r\n");
        help.concat("    * Project commands:\r\n");
        String show = "\r\n";
        show.concat(_helpProjectCmds);
        show.replace("\n", "\n    ");  // ident this
        help.concat(show);
    }

#ifdef DEBUGGER_ENABLED
    // Get help for the debugger
    if (_callbackDbgHelp) {
        help.concat("\r\n");
        help.concat(_callbackDbgHelp());
    }
#endif
    help.concat("\r\n");

#if not WEBSOCKET_DISABLED
    if (!_wsTransport.isConnected()) {  // For telnet only
        help.concat("****\r\n");
        help.concat("* New features available:\r\n");
        help.concat("* - Now you can debug in web browser too.\r\n");
        help.concat("*   Please access: http://joaolopesf.net/remotedebugapp\r\n");
        help.concat("* - Now you can add an simple software debuggger.\r\n");
        help.concat("*   Please access: https://github.com/JoaoLopesF/RemoteDebugger\r\n");
        help.concat("****\r\n");
    }
#endif

    help.concat("\r\n");
    help.concat("* Please type the command and press enter to execute.(? or h for this help)\r\n");
    help.concat("***\r\n");

    // Send to client
    debugPrint(help);
}

// Get last command received

String RemoteDebug::getLastCommand() {
    return _lastCommand;
}

// Clear the last command received

void RemoteDebug::clearLastCommand() {
    _lastCommand = "";
}

// Process user command over telnet or

void RemoteDebug::processCommand() {
    static uint32_t lastTime = 0;

    // Bug -> sometimes the command is processed twice
    // Workaround -> check time
    // TODO: see correction for this

    if (lastTime > 0 && (millis() - lastTime) < COMMAND_REPEAT_FILTER_MS) {
        debugPrintln("* Bug workaround: ignoring command repeating");
        return;
    }
    lastTime = millis();

    D("processCommand cmd: %s", _command.c_str());

    // Password request ? - 18/07/18
    if (_password != "" && !_passwordOk) {  // Process the password - 18/08/18 - adjust in 04/09/08 and 2018-10-19

        if (_command == _password) {
            debugPrintln("* Password ok, allowing access now...");

            _passwordOk = true;

#ifdef ALPHA_VERSION                                      // In test, not good yet
            sendTelnetCommand(TELNET_WILL, TELNET_ECHO);  // Send a command to telnet to restore echoes = 18/08/18
#endif
            showHelp();
        } else {
            debugPrintln("* Wrong password!");

#ifdef REMOTEDEBUG_PWD_ATTEMPTS
            _passwordAttempt++;

            if (_passwordAttempt > REMOTEDEBUG_PWD_ATTEMPTS) {
                debugPrintln("* Many attempts. Closing session now.");

                // Disconnect

                disconnect();

            } else {
                showHelp();
            }
#endif
        }

        return;
    }

    // Process commands
    debugPrint("* Debug: Command received: ");
    debugPrintln(_command);

    // Set time of last command received
    _lastTimeCommand = millis();

    // Parse the command
    CommandParser::Result parsed = _parser.parse(_command);
    const String& options = parsed.argument;

    // Get out of silent mode (unless command is silence toggle)
    if (parsed.command != CommandParser::Command::ToggleSilence && _state.isSilence()) {
        silence(false, true);
    }

    // Process the command using parsed result
    switch (parsed.command) {
    case CommandParser::Command::Help:
        D("processCommand: show help")
        showHelp();
        break;

    case CommandParser::Command::Quit:
        debugPrintln("* Closing client connection ...");
        _telnetTransport.disconnect();
        break;

    case CommandParser::Command::Memory: {
        uint32_t free = ESP.getFreeHeap();

        debugPrint("* Free Heap RAM: ");
        debugPrintln(String(free));

#if not WEBSOCKET_DISABLED

        // Send status to app
        if (_wsTransport.isConnected()) {
            wsPrintf("$app:M:%du:", free);
        }

#endif
        break;
    }

#if defined(ESP8266)
    case CommandParser::Command::Cpu80:
        // Change ESP8266 CPU to 80 MHz
        system_update_cpu_freq(80);
        debugPrintln("CPU ESP8266 changed to: 80 MHz");
        break;

    case CommandParser::Command::Cpu160:
        // Change ESP8266 CPU to 160 MHz
        system_update_cpu_freq(160);
        debugPrintln("CPU ESP8266 changed to: 160 MHz");
        break;
#endif

    case CommandParser::Command::LevelVerbose:
        _state.setLevel(VERBOSE);
        debugPrintln("* Debug level set to Verbose");
#if not WEBSOCKET_DISABLED
        wsSendLevelInfo();
#endif
        break;

    case CommandParser::Command::LevelDebug:
        _state.setLevel(DEBUG);
        debugPrintln("* Debug level set to Debug");
#if not WEBSOCKET_DISABLED
        wsSendLevelInfo();
#endif
        break;

    case CommandParser::Command::LevelInfo:
        _state.setLevel(INFO);
        debugPrintln("* Debug level set to Info");
#if not WEBSOCKET_DISABLED
        wsSendLevelInfo();
#endif
        break;

    case CommandParser::Command::LevelWarning:
        _state.setLevel(WARNING);
        debugPrintln("* Debug level set to Warning");
#if not WEBSOCKET_DISABLED
        wsSendLevelInfo();
#endif
        break;

    case CommandParser::Command::LevelError:
        _state.setLevel(ERROR);
        debugPrintln("* Debug level set to Error");
#if not WEBSOCKET_DISABLED
        wsSendLevelInfo();
#endif
        break;

    case CommandParser::Command::ToggleLevel:
        _state.setShowDebugLevel(!_state.showDebugLevel());
        debugPrintf("* Show debug level: %s\r\n",
                    (_state.showDebugLevel()) ? "On" : "Off");
        break;

    case CommandParser::Command::ToggleTime:
        _state.setShowTime(!_state.showTime());
        debugPrintf("* Show time: %s\r\n", (_state.showTime()) ? "On" : "Off");
        break;

    case CommandParser::Command::Timeout:
        if (options.length() > 0) {
            if ((options.toInt() >= 60) || (options.toInt() == 0)) {
                connectionTimeout = options.toInt() * 1000;
            } else {
                debugPrintf("* Connection Timeout must be minimal 60 seconds.\r\n");
            }
        }
        debugPrintf("* Connection Timeout: %d seconds (0=disabled)\r\n", connectionTimeout / 1000);
        break;

    case CommandParser::Command::ToggleSilence:
        silence(!_state.isSilence());
        break;

    case CommandParser::Command::ToggleProfiler:
        _state.setShowProfiler(!_state.showProfiler());
        _state.setMinTimeShowProfiler(0);
        debugPrintf("* Show profiler: %s\r\n",
                    (_state.showProfiler()) ? "On" : "Off");
        break;

    case CommandParser::Command::ProfilerMin:
        if (options.length() > 0) {
            int32_t aux = options.toInt();
            if (aux > 0) {
                _state.setShowProfiler(true);
                _state.setMinTimeShowProfiler(aux);
                debugPrintf("* Show profiler: On (with minimal time: %u)\r\n", _state.minTimeShowProfiler());
            }
        }
        break;

    case CommandParser::Command::ProfilerLevel:
        _state.setLevelBeforeProfiler(_state.getLevel());
        _state.setLevel(PROFILER);

        if (_state.showProfiler() == false) {
            _state.setShowProfiler(true);
        }

        _state.setLevelProfilerDisable(PROFILER_DEFAULT_TIMEOUT_MS);  // Default

        if (options.length() > 0) {
            int32_t aux = options.toInt();
            if (aux > 0) {
                _state.setLevelProfilerDisable(millis() + aux);
            }
        }

        debugPrintf("* Debug level set to Profiler (disable in %u millis)\r\n", _state.getLevelProfilerDisable());
        break;

    case CommandParser::Command::AutoProfiler:
        _state.setAutoLevelProfiler(AUTO_PROFILER_DEFAULT_MS);  // Default

        if (options.length() > 0) {
            int32_t aux = options.toInt();
            if (aux > 0) {
                _state.setAutoLevelProfiler(aux);
            }
        }

        debugPrintf("* Auto profiler debug level active (time >= %u millis)\r\n", _state.getAutoLevelProfiler());
        break;

    case CommandParser::Command::ToggleColors:
        _state.setShowColors(!_state.showColors());
        debugPrintf("* Show colors: %s\r\n", (_state.showColors()) ? "On" : "Off");
        break;

    case CommandParser::Command::Filter:
        setFilter(options);
        break;

    case CommandParser::Command::NoFilter:
        setNoFilter();
        break;

    case CommandParser::Command::Reset:
        if (_resetCommandEnabled) {
            debugPrintln("* Reset ...");
            debugPrintln("* Closing client connection ...");

#if defined(ESP8266)
            debugPrintln("* Resetting the ESP8266 ...");
#elif defined(ESP32)
            debugPrintln("* Resetting the ESP32 ...");
#endif

            _telnetTransport.disconnect();
            _telnetTransport.stop();

#if not WEBSOCKET_DISABLED
            _wsTransport.stop();
#endif

            delay(RESET_DELAY_MS);

            // Reset
            ESP.restart();
        }
        break;

#ifdef DEBUGGER_ENABLED
    case CommandParser::Command::Debugger:
        if (_callbackDbgProcessCmd) {
            _callbackDbgProcessCmd();
        } else {
            debugPrintln("* RemoteDebugger not activate for this project");
            debugPrintln("* Please access it to see how activate this:");
            debugPrintln("* https://github.com/JoaoLopesF/RemoteDebugger");
        }
        break;
#endif

    case CommandParser::Command::Custom:
    case CommandParser::Command::None:
    default:
        // Callbacks

#ifdef DEBUGGER_ENABLED
        // Process commands for the debugger
        if (_callbackDbgProcessCmd) {
            _callbackDbgProcessCmd();
        }
#endif

        // Project commands - set by programmer
        if (_callbackProjectCmds) {
            _callbackProjectCmds();
        }
        break;
    }
}

// Filter

void RemoteDebug::setFilter(String filter) {
    filter.toLowerCase();  // TODO: option to case insensitive ?
    _state.setFilter(filter);

    debugPrint("* Debug: Filter active: ");
    debugPrintln(_state.filter());
}

void RemoteDebug::setNoFilter() {
    _state.clearFilter();

    debugPrintln("* Debug: Filter disabled");
}

// Silence

void RemoteDebug::silence(boolean activate, boolean showMessage, boolean fromBreak, uint32_t timeout) {
    // Set silence and timeout

    if (showMessage) {
        if (activate) {
            debugPrintln("* Debug now is in silent mode!");
#if not WEBSOCKET_DISABLED
            if (_wsTransport.isConnected()) {
                debugPrintln("* Press button \"Silence\" or another command to return show debugs");
            } else {
                debugPrintln("* Press s again or another command to return show debugs");
            }
#else
            debugPrintln("* Press s again or another command to return show debugs");
#endif
        } else {
            debugPrintln("* Debug now exit from silent mode!");
        }
    }

    // Set it

    _state.setSilence(activate);
    _state.setSilenceTimeout((timeout == 0) ? 0 : (millis() + timeout));

#if not WEBSOCKET_DISABLED

    // Send status to app

    if (_wsTransport.isConnected()) {
        wsPrintf("$app:S:%c", ((_state.isSilence()) ? '1' : '0'));
    }

#endif
}

boolean RemoteDebug::isSilence() {
    return _state.isSilence();
}

// Format numbers

String RemoteDebug::formatNumber(uint32_t value, uint8_t size, char insert) {
    // Putting zeroes in left

    String ret = "";

    for (uint8_t i = 1; i <= size; i++) {
        uint32_t max = pow(10, i);
        if (value < max) {
            for (uint8_t j = (size - i); j > 0; j--) {
                ret.concat(insert);
            }
            break;
        }
    }

    ret.concat(value);

    return ret;
}

#if not WEBSOCKET_DISABLED

///////  routines

// Write string to WebSocket transport
void RemoteDebug::wsWrite(const String& str) {
    _wsTransport.write((const uint8_t*)str.c_str(), str.length());
}

// Process user command over telnet or

void RemoteDebug::wsOnReceive(const char* command) {  // @suppress("Unused function declaration")

    // Process the command

    _command = command;
    _lastCommand = _command;  // Store the last command

    D("wsOnReceive cmd: %s", command);

    // Is app commands
    if (_command == "$app") {
        D("wsOnReceive app command")
        // RemoteDebug connected, send info
        wsSendInfo();
    } else {  // Normal commands
        processCommand();
    }
}

// Send info to RemoteDebugApp

/**
 * @brief Send version, board, debugger disabled and  if is low or enough memory board.
 *
 */
void RemoteDebug::wsSendInfo() {
    char features;
    char dbgEnabled;

    // Not connected ?
    if (!_wsTransport.isConnected()) {
        D("wsSendInfo not connected")
        return;
    }

    // Features
#ifndef DEBUGGER_ENABLED
    features = 'M';  // Disabled
    dbgEnabled = 'D';
#else
    if (_callbackDbgProcessCmd) {
        features = 'E';  // Enough
        dbgEnabled = 'E';
    } else {
        features = 'M';  // Medium
        dbgEnabled = 'D';
    }
#endif

    // Send info
    String version = String(VERSION);
    String board;

#ifdef ESP32
    board = "ESP32";
#else
    board = "ESP8266";
#endif

    wsPrintln("");  // Workaround to not get dirty "[0m" ???
    wsPrintf("$app:V:%s:%s:%c:%du:%c:N", version.c_str(), board.c_str(), features, getFreeMemory(), dbgEnabled);

    // Status of debug level
    wsSendLevelInfo();

    // Send status of debugger
    // TODO: made it
}

void RemoteDebug::wsSendLevelInfo() {
    // Send debug level info to app
    if (_wsTransport.isConnected()) {
        wsPrintf("$app:L:%u", _state.getLevel());
    }
}

#endif  // WEBSOCKET_DISABLED

boolean RemoteDebug::wsIsConnected() {
    //  is connected (RemoteDebugApp)

#if not WEBSOCKET_DISABLED
    return _wsTransport.isConnected();
#else
    return false;
#endif
}

/////// Utilities

// Get free memory

uint32_t RemoteDebug::getFreeMemory() {
    return ESP.getFreeHeap();
}

// Is CR or LF ?
boolean RemoteDebug::isCRLF(char character) {
    return (character == '\r' || character == '\n');
}

// Expand characters as CR/LF to \\r, \\n
// TODO: make this for another chars not printable
String RemoteDebug::expand(String string) {
    string.replace("\r", "\\r");
    string.replace("\n", "\\n");

    return string;
}

#ifdef ALPHA_VERSION  // In test, not good yet
// Send telnet commands (as used with password request) - 18/08/18
// Experimental code !

void RemoteDebug::sendTelnetCommand(uint8_t command, uint8_t option) {
    // Send a command to the telnet client

    debugPrintf("%c%c%c", TELNET_IAC, command, option);
    WiFiClient* client = getTelnetClient();
    if (client) {
        client->flush();
    }
}
#endif

#else                     // DEBUG_DISABLED

/////// All debug is disabled; this include defines empty debug macros
#include "RemoteDebug.h"  // This library

#endif  // DEBUG_DISABLED
