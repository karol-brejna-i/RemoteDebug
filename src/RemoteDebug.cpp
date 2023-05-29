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

#if not WEBSOCKET_DISABLED
#include "RemoteDebugWS.h"
#endif

// Internal debug macro - recommended stay disable
#define D(fmt, ...)
// use the following line to enable debug
// #define D(fmt, ...) Serial.printf("rd: " fmt "\n", ##__VA_ARGS__);  // Serial debug

// Internal print macros for send messages to client

#if not WEBSOCKET_DISABLED

#define debugPrintf(fmt, ...)                        \
    {                                                \
        if (_connected)                              \
            TelnetClient.printf(fmt, ##__VA_ARGS__); \
        else if (_connectedWS)                       \
            DebugWS.printf(fmt, ##__VA_ARGS__);      \
    }
#define debugPrintln(str)              \
    {                                  \
        if (_connected)                \
            TelnetClient.println(str); \
        else if (_connectedWS)         \
            DebugWS.println(str);      \
    }
#define debugPrint(str)              \
    {                                \
        if (_connected)              \
            TelnetClient.print(str); \
        else if (_connectedWS)       \
            DebugWS.print(str);      \
    }

#else  // With  too

#define debugPrintf(fmt, ...)                                    \
    {                                                            \
        if (_connected) TelnetClient.printf(fmt, ##__VA_ARGS__); \
    }
#define debugPrintln(str)                          \
    {                                              \
        if (_connected) TelnetClient.println(str); \
    }
#define debugPrint(str)                          \
    {                                            \
        if (_connected) TelnetClient.print(str); \
    }

#endif

// Instance
static RemoteDebug* _instance;

// WiFi server (telnet)
static WiFiServer TelnetServer(TELNET_PORT);  // @suppress("Abstract class cannot be instantiated")
static WiFiClient TelnetClient;               // @suppress("Abstract class cannot be instantiated")

// Support to websocket connection with RemoteDebugApp
#if not WEBSOCKET_DISABLED

// Instance of RemoteDebugWS
static RemoteDebugWS DebugWS;  // @suppress("Abstract class cannot be instantiated")

static boolean _connectedWS = false;  // Connected :

// Callbacks
class MyRemoteDebugCallbacks : public RemoteDebugWSCallbacks {
    void onConnect() {
        // WebSocket (app) connected
        D("rd: onconnect")

        _connectedWS = true;

        // Is telnet connected -> disconnect it, due reduce overheads
        if (_instance->isConnected()) {
            D("disconnect telnet, because WS is connected")
            _instance->disconnect(true);
        }

        // Call same routine that telnet
        _instance->onConnection(true);
    }

    void onDisconnect() {
        //  (app) disconnected

        D("rd: ondisconnect")
        _connectedWS = false;
    }

    void onReceive(const char* message) {
        // Receive a message
        D("rd: onreceive")
        _instance->wsOnReceive(message);
    }
};

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
    if (port != TELNET_PORT) {  // Bug: not more can use begin(port)..
        return false;
    }

    TelnetServer.begin();
    TelnetServer.setNoDelay(true);

    D("WEB_SOCKETS_DISABLED: %d", WEBSOCKET_DISABLED)
#if not WEBSOCKET_DISABLED
    // Initialize  (for RemoteDebugApp)
    DebugWS.begin(new MyRemoteDebugCallbacks());
#endif

    // Reserve space to buffer of print writes

    _bufferPrint.reserve(BUFFER_PRINT);

#ifdef CLIENT_BUFFERING
    // Reserve space to buffer of send

    _bufferPrint.reserve(MAX_SIZE_SEND);

#endif

    // Host name of this device

    _hostName = hostName;

    // Debug level

    _clientDebugLevel = startingDebugLevel;
    _lastDebugLevel = startingDebugLevel;

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
    return &TelnetClient;
}

#endif

// Set the password for telnet - thanks @jeroenst for suggest thist method

void RemoteDebug::setPassword(String password) {
    _password = password;
}

// Destructor

RemoteDebug::~RemoteDebug() {
    // Flush
    if (TelnetClient && TelnetClient.connected()) {
        TelnetClient.flush();
    }

    // Stop
    stop();
}

// Stop the server

void RemoteDebug::stop() {
    D("rd: stop")
    // Stop Client
    if (TelnetClient && TelnetClient.connected()) {
        TelnetClient.stop();
    }

    // Stop server

    TelnetServer.stop();

#if not WEBSOCKET_DISABLED
    // Stop  (RemoteDebugApp)

    DebugWS.stop();  // stop the websocket server
#endif
}

// Handle the connection (in begin of loop in sketch)
// TODO: optimize when loop not have a large delay
void RemoteDebug::handle() {
#ifdef ALPHA_VERSION  // In test, not good yet
    static uint32_t lastTime = millis();
#endif

#ifdef DEBUGGER_ENABLED
    static uint32_t dbgTimeHandle = millis();  // To avoid call the handler desnecessary
    static boolean dbgLastConnected = false;   // Last is connected ?
#endif

    // Silence timeout ?

    if (_silence && _silenceTimeout > 0 && millis() >= _silenceTimeout) {
        // Get out of silence mode
        silence(false, true);
    }

    // Debug level is profiler -> set the level before
    if (_clientDebugLevel == PROFILER) {
        if (millis() > _levelProfilerDisable) {
            _clientDebugLevel = _levelBeforeProfiler;
            debugPrintln("* Debug level profile inactive now");
        }
    }

#ifdef ALPHA_VERSION  // In test, not good yet

    // Automatic change to profiler level if time between handles is greater than n millis
    if (_autoLevelProfiler > 0 && _clientDebugLevel != PROFILER) {
        uint32_t diff = (millis() - lastTime);

        if (diff >= _autoLevelProfiler) {
            _levelBeforeProfiler = _clientDebugLevel;
            _clientDebugLevel = PROFILER;
            _levelProfilerDisable = 1000;  // Disable it at 1 sec

            debugPrintf("* Debug level profile active now - time between handels: %u\r\n", diff);
        }

        lastTime = millis();
    }
#endif

    // look for Client connect trial
    if (TelnetServer.hasClient()) {
        // Old connection logic

        //      if (!TelnetClient || !TelnetClient.connected()) {
        //
        //        if (TelnetClient) { // Close the last connect - only one supported
        //
        //          TelnetClient.stop();
        //
        //        }

        // New connection logic - 10/08/17

        if (TelnetClient && TelnetClient.connected()) {
            // Verify if the IP is same than actual conection

            WiFiClient newClient;  // @suppress("Abstract class cannot be instantiated")
            newClient = TelnetServer.available();
            String ip = newClient.remoteIP().toString();

            if (ip == TelnetClient.remoteIP().toString()) {
                // Reconnect

                TelnetClient.stop();
                TelnetClient = newClient;

            } else {
                // Disconnect (not allow more than one connection)
                newClient.stop();

                return;
            }

        } else {
            // New TCP client

            TelnetClient = TelnetServer.available();

            // Password request ? - 18/07/18

            if (_password != "") {
#ifdef ALPHA_VERSION  // In test, not good yet
                      // Send command to telnet client to not do local echos
                      // Experimental code !

                sendTelnetCommand(TELNET_WONT, TELNET_ECHO);
#endif
            }
        }

        if (!TelnetClient) {  // No client yet ???
            return;
        }

        // Set client
        TelnetClient.setNoDelay(true);  // More faster
        TelnetClient.flush();           // clear input buffer, else you get strange characters

        // Empty buffer
        delay(100);

        while (TelnetClient.available()) {
            TelnetClient.read();
        }

        // Connection event
        onConnection(true);
    }

    // Is client connected ? (to reduce overhead in active)
    _connected = (TelnetClient && TelnetClient.connected());

    // Get command over telnet
    if (_connected) {
        char last = ' ';  // To avoid process two times the "\r\n"

        while (TelnetClient.available()) {  // get data from Client

            // Get character
            char character = TelnetClient.read();

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
    boolean connected = (_connected || _connectedWS);
#else  // By telnet
    boolean connected = _connected;
#endif

    if (connected) {
#ifdef CLIENT_BUFFERING
        // Client buffering - send data in intervals to avoid delays or if its is too big

        if ((millis() - _lastTimeSend) >= DELAY_TO_SEND || _sizeBufferSend >= MAX_SIZE_SEND) {
            debugPrint(_bufferSend);
            _bufferSend = "";
            _sizeBufferSend = 0;
            _lastTimeSend = millis();
        }
#endif

#ifdef MAX_TIME_INACTIVE
#if MAX_TIME_INACTIVE > 0

        // Inactivity - close connection if not received commands from user in telnet
        // For reduce overheads

        uint32_t maxTime = MAX_TIME_INACTIVE;  // Normal

        if (_password != "" && !_passwordOk) {  // Request password - 18/08/08
            maxTime = 60000;                    // One minute to password
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

    //  server handle

    DebugWS.handle();

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
            TelnetClient.println("* Closing client connection ...");  // this is to web app new conn not receive it
        }
    } else {
        debugPrintln("* Closing client connection ...");
    }

    _silence = false;
    _silenceTimeout = 0;

    if (_connected) {  // By telnet
        TelnetClient.stop();
        _connected = false;
    }
#if not WEBSOCKET_DISABLED
    if (_connectedWS && !onlyTelnetClient) {
        D("rd DebugWS.disconnect()")
        DebugWS.disconnect();  // Disconnect client
        _connectedWS = false;
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
    _lastTimePrint = millis();    // Clear the time
    _silence = false;             // No silence
    _silenceTimeout = 0;

#ifdef CLIENT_BUFFERING
    // Client buffering - send data in intervals to avoid delays or if its is too big
    _bufferSend = "";
    _sizeBufferSend = 0;
    _lastTimeSend = millis();
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
    return (_connected || _connectedWS);
#else
    return _connected;
#endif
}

// Send to serial too (use only if need)
void RemoteDebug::setSerialEnabled(boolean enable) {
    _serialEnabled = enable;
    _showColors = false;  // Disable it for Serial
}

// Allow ESP reset over telnet client

void RemoteDebug::setResetCmdEnabled(boolean enable) {
    _resetCommandEnabled = enable;
}

// Show time in millis

void RemoteDebug::showTime(boolean show) {
    _showTime = show;
}

// Show profiler - time in millis between messages of debug

void RemoteDebug::showProfiler(boolean show, uint32_t minTime) {
    _showProfiler = show;
    _minTimeShowProfiler = minTime;
}

#ifdef ALPHA_VERSION  // In test, not good yet
// Automatic change to profiler level if time between handles is greater than n mills (0 - disable)

void RemoteDebug::autoProfilerLevel(uint32_t millisElapsed) {
    _autoLevelProfiler = millisElapsed;
}
#endif

// Show debug level

void RemoteDebug::showDebugLevel(boolean show) {
    _showDebugLevel = show;
}

// Show colors

void RemoteDebug::showColors(boolean show) {
    if (_serialEnabled == false) {
        _showColors = show;
    } else {
        _showColors = false;  // Disable it for Serial
    }
}

// Show in raw mode - only data ?

void RemoteDebug::showRaw(boolean show) {
    _showRaw = show;
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
    boolean ret = (debugLevel >= _clientDebugLevel &&
                   !_silence &&
                   (_connected || _connectedWS || _serialEnabled));
#else  // Telnet only
    boolean ret = (debugLevel >= _clientDebugLevel &&
                   !_silence &&
                   (_connected || _serialEnabled));
#endif
    if (ret) {
        _lastDebugLevel = debugLevel;
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
    boolean connected = (_connected || _connectedWS);
#else
    boolean connected = _connected;
#endif

    // In silent mode now ?
    if (_silence) {
        return 0;
    }

    // New line writted before ?
    if (_newLine) {
#ifdef DEBUGGER_ENABLED
        // For Simple software debugger - based on SerialDebug Library
        // Changed handle debugger logic - 2018-02-29
        if (!_showRaw) {  // Not for raw mode

            if (_callbackDbgEnabled && _callbackDbgEnabled()) {  // Callbacks ok

                if (connected && _callbackDbgEnabled()) {  // Only call if is connected and debugger is enabled
                    // Call the handle
                    _callbackDbgHandle(false);
                }
            }
        }
#endif

        String show = "";

        // Not in raw mode (only data)
        if (!_showRaw) {
            // New color system
            if (_showColors) {
                switch (_lastDebugLevel) {
                    case VERBOSE:
                        show = COLOR_VERBOSE;
                        break;
                    case DEBUG:
                        show = COLOR_DEBUG;
                        break;
                    case INFO:
                        show = COLOR_INFO;
                        break;
                    case WARNING:
                        show = COLOR_WARNING;
                        break;
                    case ERROR:
                        show = COLOR_ERROR;
                        break;
                }
                colorLevel = show;
            }

            // Show debug level
            if (_showDebugLevel) {
                switch (_lastDebugLevel) {
                    case PROFILER:
                        show.concat("(P");
                        break;
                    case VERBOSE:
                        show.concat("(V");
                        break;
                    case DEBUG:
                        show.concat("(D");
                        break;
                    case INFO:
                        show.concat("(I");
                        break;
                    case WARNING:
                        show.concat("(W");
                        break;
                    case ERROR:
                        show.concat("(E");
                        break;
                }
            }

            // Show time in millis
            if (_showTime) {
                if (show != "") {
                    show.concat(" ");
                }
                show.concat("t:");
                show.concat(millis());
                show.concat("ms");
            }

            // Show profiler (time between messages)
            if (_showProfiler) {
                elapsed = (millis() - _lastTimePrint);
                boolean resetColors = false;
                if (show != "") {
                    show.concat(" ");
                }
                if (_showColors) {
                    if (elapsed < 250) {
                        ;  // not color this
                    } else if (elapsed < 1000) {
                        show.concat(COLOR_BLACK);
                        show.concat(COLOR_BACKGROUND_GREEN);
                        resetColors = true;
                    } else if (elapsed < 3000) {
                        show.concat(COLOR_BLACK);
                        show.concat(COLOR_BACKGROUND_YELLOW);
                        resetColors = true;
                    } else if (elapsed < 5000) {
                        show.concat(COLOR_WHITE);
                        show.concat(COLOR_BACKGROUND_MAGENTA);
                        resetColors = true;
                    } else {
                        show.concat(COLOR_WHITE);
                        show.concat(COLOR_BACKGROUND_RED);
                        resetColors = true;
                    }
                }
                show.concat("p:^");
                show.concat(formatNumber(elapsed, 4));
                show.concat("ms");
                if (resetColors) {
                    show.concat(COLOR_RESET);
                    show.concat(colorLevel);
                }
                _lastTimePrint = millis();
            }
        } else {  // Raw mode - only data - e.g. used for debugger messages
            show.concat(COLOR_RAW);
        }

        // Show anything ?
        if (show != "") {
            if (!_showRaw) {
                show.concat(") ");
            }

            // Write to telnet buffered
            if (connected || _serialEnabled) {  // send data to Client
                _bufferPrint = show;
            }
        }

        _newLine = false;
    }

    // Print ?
    boolean doPrint = false;

    // New line ?
    if (character == '\n') {
        _bufferPrint.concat("\r");  // Para clientes windows - 29/01/17

        _newLine = true;
        doPrint = true;

    } else if (_bufferPrint.length() == BUFFER_PRINT) {  // Limit of buffer
        doPrint = true;
    }

    // Write to telnet Buffered
    _bufferPrint.concat((char)character);

    // Send the characters buffered by print.h
    if (doPrint) {  // Print the buffer
        boolean noPrint = false;

        if (_showProfiler && elapsed < _minTimeShowProfiler) {  // Profiler time Minimal
            noPrint = true;
        } else if (_filterActive) {  // Check filter before print

            String aux = _bufferPrint;
            aux.toLowerCase();

            if (aux.indexOf(_filter) == -1) {  // not find -> no print
                noPrint = true;
            }
        }

        if (noPrint == false) {
            if (_showColors) _bufferPrint.concat(COLOR_RESET);
            // Send to telnet or websocket (buffered)
            boolean sendToClient = connected;

            if (_password != "" && !_passwordOk) {  // With no password -> no telnet output - 2018-10-19
                sendToClient = false;
            }

            if (sendToClient) {  // send data to Client

#ifndef CLIENT_BUFFERING
                debugPrint(_bufferPrint);
#else  // Client buffering
                uint8_t size = _bufferPrint.length();

                // Buffer too big ?
                if ((_sizeBufferSend + size) >= MAX_SIZE_SEND) {
                    // Send it
                    debugPrint(_bufferSend);
                    _bufferSend = "";
                    _sizeBufferSend = 0;
                    _lastTimeSend = millis();
                }

                // Add to buffer of send
                _bufferSend.concat(_bufferPrint);
                _sizeBufferSend += size;

                // Client buffering - send data in intervals to avoid delays or if its is too big
                // Not for raw mode
                if (_showRaw || (millis() - _lastTimeSend) >= DELAY_TO_SEND) {
                    debugPrint(_bufferSend);
                    _bufferSend = "";
                    _sizeBufferSend = 0;
                    _lastTimeSend = millis();
                }
#endif
            }

            // Echo to serial (not buffering it)
            if (_serialEnabled) {
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
    if (!_connectedWS) {  // For telnet only
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

    // Bug -> sometimes the command is process twice
    // Workaround -> check time
    // TODO: see correction for this

    if (lastTime > 0 && (millis() - lastTime) < 500) {
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

    String options = "";
    uint8_t pos = _command.indexOf(" ");
    if (pos > 0) {
        options = _command.substring(pos + 1);
    }

    // Set time of last command received
    _lastTimeCommand = millis();

    // Get out of silent mode
    if (_command != "s" && _silence) {
        silence(false, true);
    }

    // Process the command
    if (_command == "h" || _command == "?") {
        // Show help
        D("processCommand: show help")
        showHelp();

    } else if (_command == "q") {
        // Quit

        debugPrintln("* Closing client connection ...");

        TelnetClient.stop();

    } else if (_command == "m") {
        uint32_t free = ESP.getFreeHeap();

        debugPrint("* Free Heap RAM: ");
        debugPrintln(free);

#if not WEBSOCKET_DISABLED

        // Send status to app
        if (_connectedWS) {
            DebugWS.printf("$app:M:%du:\n", free);
        }

#endif

#if defined(ESP8266)
    } else if (_command == "cpu80") {
        // Change ESP8266 CPU to 80 MHz
        system_update_cpu_freq(80);
        debugPrintln("CPU ESP8266 changed to: 80 MHz");
    } else if (_command == "cpu160") {
        // Change ESP8266 CPU to 160 MHz
        system_update_cpu_freq(160);
        debugPrintln("CPU ESP8266 changed to: 160 MHz");

#endif

    } else if (_command == "v") {
        // Debug level

        _clientDebugLevel = VERBOSE;

        debugPrintln("* Debug level set to Verbose");

#if not WEBSOCKET_DISABLED
        wsSendLevelInfo();
#endif

    } else if (_command == "d") {
        // Debug level

        _clientDebugLevel = DEBUG;

        debugPrintln("* Debug level set to Debug");

#if not WEBSOCKET_DISABLED
        wsSendLevelInfo();
#endif

    } else if (_command == "i") {
        // Debug level

        _clientDebugLevel = INFO;

        debugPrintln("* Debug level set to Info");

#if not WEBSOCKET_DISABLED
        wsSendLevelInfo();
#endif

    } else if (_command == "w") {
        // Debug level

        _clientDebugLevel = WARNING;

        debugPrintln("* Debug level set to Warning");

#if not WEBSOCKET_DISABLED
        wsSendLevelInfo();
#endif

    } else if (_command == "e") {
        // Debug level

        _clientDebugLevel = ERROR;

        debugPrintln("* Debug level set to Error");

#if not WEBSOCKET_DISABLED
        wsSendLevelInfo();
#endif

    } else if (_command == "l") {
        // Show debug level

        _showDebugLevel = !_showDebugLevel;

        debugPrintf("* Show debug level: %s\r\n",
                    (_showDebugLevel) ? "On" : "Off");

    } else if (_command == "t") {
        // Show time

        _showTime = !_showTime;

        debugPrintf("* Show time: %s\r\n", (_showTime) ? "On" : "Off");

    } else if (_command.startsWith("timeout")) {
        // Set or get connection timeout

        if (options.length() > 0) {  // With minimal time
            if ((options.toInt() >= 60) || (options.toInt() == 0)) {
                connectionTimeout = options.toInt() * 1000;
            } else {
                debugPrintf("* Connection Timeout must be minimal 60 seconds.\r\n");
            }
        }
        debugPrintf("* Connection Timeout: %d seconds (0=disabled)\r\n", connectionTimeout / 1000);

    } else if (_command == "s") {
        // Toogle silence (new) = 28/08/18

        silence(!_silence);

    } else if (_command == "p") {
        // Show profiler

        _showProfiler = !_showProfiler;
        _minTimeShowProfiler = 0;

        debugPrintf("* Show profiler: %s\r\n",
                    (_showProfiler) ? "On" : "Off");

    } else if (_command.startsWith("p ")) {
        // Show profiler with minimal time
        if (options.length() > 0) {  // With minimal time
            int32_t aux = options.toInt();
            if (aux > 0) {  // Valid number
                _showProfiler = true;
                _minTimeShowProfiler = aux;
                debugPrintf("* Show profiler: On (with minimal time: %u)\r\n", _minTimeShowProfiler);
            }
        }

    } else if (_command == "P") {
        // Debug level profile
        _levelBeforeProfiler = _clientDebugLevel;
        _clientDebugLevel = PROFILER;

        if (_showProfiler == false) {
            _showProfiler = true;
        }

        _levelProfilerDisable = 1000;  // Default

        if (options.length() > 0) {  // With time of disable
            int32_t aux = options.toInt();
            if (aux > 0) {  // Valid number
                _levelProfilerDisable = millis() + aux;
            }
        }

        debugPrintf("* Debug level set to Profiler (disable in %u millis)\r\n", _levelProfilerDisable);

    } else if (_command == "A") {
        // Auto debug level profile

        _autoLevelProfiler = 1000;  // Default

        if (options.length() > 0) {  // With time of disable
            int32_t aux = options.toInt();
            if (aux > 0) {  // Valid number
                _autoLevelProfiler = aux;
            }
        }

        debugPrintf("* Auto profiler debug level active (time >= %u millis)\r\n", _autoLevelProfiler);

    } else if (_command == "c") {
        // Show colors

        _showColors = !_showColors;
        debugPrintf("* Show colors: %s\r\n", (_showColors) ? "On" : "Off");

    } else if (_command.startsWith("filter ") && options.length() > 0) {
        setFilter(options);

    } else if (_command == "nofilter") {
        setNoFilter();
    } else if (_command == "reset" && _resetCommandEnabled) {
        debugPrintln("* Reset ...");

        debugPrintln("* Closing client connection ...");

#if defined(ESP8266)
        debugPrintln("* Resetting the ESP8266 ...");
#elif defined(ESP32)
        debugPrintln("* Resetting the ESP32 ...");
#endif

        TelnetClient.stop();
        TelnetServer.stop();

#if not WEBSOCKET_DISABLED
        DebugWS.stop();
#endif

        delay(500);

        // Reset

        ESP.restart();

#ifdef DEBUGGER_ENABLED

    } else if (!_callbackDbgProcessCmd && _command.startsWith("dbg")) {
        // Show a message of debugger not is active

        debugPrintln("* RemoteDebugger not activate for this project");
        debugPrintln("* Please access it to see how activate this:");
        debugPrintln("* https://github.com/JoaoLopesF/RemoteDebugger");

#endif

    } else {
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
    }
}

// Filter

void RemoteDebug::setFilter(String filter) {
    _filter = filter;
    _filter.toLowerCase();  // TODO: option to case insensitive ?
    _filterActive = true;

    debugPrint("* Debug: Filter active: ");
    debugPrintln(_filter);
}

void RemoteDebug::setNoFilter() {
    _filter = "";
    _filterActive = false;

    debugPrintln("* Debug: Filter disabled");
}

// Silence

void RemoteDebug::silence(boolean activate, boolean showMessage, boolean fromBreak, uint32_t timeout) {
    // Set silence and timeout

    if (showMessage) {
        if (activate) {
            debugPrintln("* Debug now is in silent mode!");
#if not WEBSOCKET_DISABLED
            if (_connectedWS) {
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

    _silence = activate;
    _silenceTimeout = (timeout == 0) ? 0 : (millis() + timeout);

#if not WEBSOCKET_DISABLED

    // Send status to app

    if (_connectedWS) {
        DebugWS.printf("$app:S:%c\n", ((_silence) ? '1' : '0'));
    }

#endif
}

boolean RemoteDebug::isSilence() {
    return _silence;
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
    if (!_connectedWS) {
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

    DebugWS.println();  // Workaround to not get dirty "[0m" ???
    DebugWS.printf("$app:V:%s:%s:%c:%du:%c:N\n", version.c_str(), board.c_str(), features, getFreeMemory(), dbgEnabled);

    // Status of debug level
    wsSendLevelInfo();

    // Send status of debugger
    // TODO: made it
}

void RemoteDebug::wsSendLevelInfo() {
    // Send debug level info to app
    if (_connectedWS) {
        DebugWS.printf("$app:L:%u\n", _clientDebugLevel);
    }
}

#endif  // WEBSOCKET_DISABLED

boolean RemoteDebug::wsIsConnected() {
    //  is connected (RemoteDebugApp)

#if not WEBSOCKET_DISABLED
    return _connectedWS;
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
    TelnetClient.flush();
}
#endif

#else                     // DEBUG_DISABLED

/////// All debug is disabled, this include is to define empty debug macros
#include "RemoteDebug.h"  // This library

#endif  // DEBUG_DISABLED
