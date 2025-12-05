# RemoteDebug Library

A library for ESP2866 and ESP32 for debuging projects over WiFi.
[![arduino-library-badge](https://www.ardu-badge.com/badge/RemoteDebug2.svg?)](https://www.ardu-badge.com/RemoteDebug2)

RemoteDebug sets up a TCP/IP server, that you connect to using telnet or websockets (using a dedicated web app).

This project is a fork of not supported (as it seems, last update on May 9, 2019) [RemoteDebug](https://github.com/JoaoLopesF/RemoteDebug) library by Joao Lopes.
The API is fully compatible. If you have a project using the original library, you can use this one as a drop-in replacement (just change the dependency, you don't need to change the code).
In addition, we have some critical bug fixes and feature improvements.

See [old project readme](extras/old_README.md) for more background.

## Features

The same as the original library:

- Ability to see debug messages on the serial monitor
- Ability to see debug messages on the telnet client
- Ability to see debug messages on the web app (WebSockets)
- Display free memory, basic profiling info, ESP SDK version, etc.
- Ability to send predefined commands to the app -- change log level, reset the device, etc.
- Ability to send custom commands to the app -- the programmer can define some commands to be executed on the device

## Usage

See the [Simple example description](#simple-example) below to learn, how to use the library in your project.

In general, you need to do the following:

- include the library header file
- create an instance of the RemoteDebug class
- initialize the library in the setup function
- call the handle function in the loop function

When you want to log something, you can use the following single line macros:

```cpp
debugV("* This is a message of debug level VERBOSE");
debugD("* This is a message of debug level DEBUG");
debugI("* This is a message of debug level INFO");
debugW("* This is a message of debug level WARNING");
debugE("* This is a message of debug level ERROR");
```

or use a snippet like below (for longer logging fragments, and lower overhead):

```cpp
#ifndef DEBUG_DISABLED
       if (Debug.isActive(Debug.<level>)) { // change <level> to expected log level
           Debug.printf("some info: %d %s\n", number, str);
           Debug.println("more info");
       }
#endif
```

## Examples

This library comes with a few examples. They demonstrate the basic functionality of the library and how to use it.

### Simple example

This is a very basic example of RemoteDebug library usage.

> Take a look at the full source code in `examples/simple/simple.ino` file.

This example connects to WiFi (remember to set up ssid and password in the code), and initialize the RemoteDebug library.

After that, the following logic is executed:

- Each second, the built-in led blinks and a message is sent to RemoteDebug (in verbose level)
- Each 5 seconds, a message is sent to RemoteDebug in all levels (verbose, debug, info, warning and error) and a function is called
Before running, decide if you want to use mDNS (change the define USE_MDNS to true or false).

Please, see the following "video" to see how the app "looks like" when we use serial monitor (the device is connected to the computer using USB cable):

[![asciicast](https://asciinema.org/a/587829.png)](https://asciinema.org/a/587829)

And this is how it looks like when we use telnet:

[![asciicast](https://asciinema.org/a/587830.svg)](https://asciinema.org/a/587830) [![asciicast](https://asciinema.org/a/587831.svg)](https://asciinema.org/a/587831)

When you lookl at the source code you will find the following key parts:

```cpp
#include "RemoteDebug.h"
RemoteDebug Debug;
```

In order to use the library, you need to include the header file and create an instance of the RemoteDebug class.

Then, in the setup function, you need to initialize the library (after connecting to WiFi, as you would normally do):

```cpp
    Debug.begin(HOST_NAME);
    Debug.setResetCmdEnabled(true);
    Debug.showProfiler(true);
    Debug.showColors(true);
```

Finally in the loop function, you need to call the handle function:

```cpp
    Debug.handle();
```

## Custom commands example

TBD

## Limitations

The original functionality is not changed. The following limitations are inherited from the original library:

- doesn't support SSL (technical limitation of the underlying library)
- doesn't use async websockets (design choice?)
- supports either telnet or websockets, but not both at the same time (implementation choice)
- websockets logger doesn't send unicode characters (probably implementation problem)

The library has no tests, nor CI/CD.

Probably, with time some of these limitations will be removed.

### Changes from the original library

- Included changes from the following PRs (not included in the original library ATTOW):
  - <https://github.com/JoaoLopesF/RemoteDebug/pull/73>
  - <https://github.com/JoaoLopesF/RemoteDebug/pull/56>
- MAJOR CHANGE: Removed ArduinoWebsockets from the sources and used the one from the library manager. There is still "a little problem" with <https://github.com/Links2004/arduinoWebSockets> -- it doesn't compile under ESP32 (latest version 2.4.1), so we use the older version 2.3.4. This is a temporary solution, until the problem is fixed.
