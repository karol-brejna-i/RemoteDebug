# RemoteDebug Library

A library for ESP2866 and ESP32 for debuging projects over WiFi.

RemoteDebug sets up a TCP/IP server, that you connect to using telnet or using a dedicated web app.

This project is a fork of not supported (as it seems, last update on May 9, 2019) [RemoteDebug](https://github.com/JoaoLopesF/RemoteDebug) library by Joao Lopes.
The API is fully compatible. In addition, we have some critical bug fixes and feature improvements.

See [old project readme](./old_README.md) for more background.

## Features

The same as the original library.

## Examples

This library comes with a few examples. They demonstrate the basic functionality of the library and how to use it.

### [Simple example](./examples/simple/simple.ino)

This is a very basic example of RemoteDebug library usage.

This example connects to WiFi (remember to set up ssid and password in the code), and initialize the RemoteDebug library.

After that, the following logic is executed:

- Each second, the led is blinked and a message is sent to RemoteDebug (in verbose level)
- Each 5 seconds, a message is sent to RemoteDebug in all levels (verbose, debug, info, warning and error) and a function is called
Before running, decide if you want to use mDNS (change the define USE_MDNS to true or false).

Please, see the following "video" to see how the app "looks like" when we use serial monitor (the device is connected to the computer using USB cable):

[![asciicast](https://asciinema.org/a/587829.png)](https://asciinema.org/a/587829)

And this is how it looks like when we use telnet:

[![asciicast](https://asciinema.org/a/587830.svg)](https://asciinema.org/a/587830) [![asciicast](https://asciinema.org/a/587831.svg)](https://asciinema.org/a/587831)


When you lookl at the [source code]((./examples/simple/simple.ino)) you will find the following key parts:

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

## Limitations

The original functionality is not changed. The following limitations are inherited from the original library:

- doesn't support SSL (technical limitation of the underlying library)
- doesn't use async websockets (design choice?)
- supports either telnet or websockets, but not both at the same time (implementation choice)

The library has no tests, nor CI/CD.

Probably, with time some of these limitations will be removed.

### Changes from the original library

- Included changes from the following PRs (not included in the original library ATTOW):
  - <https://github.com/JoaoLopesF/RemoteDebug/pull/73>
  - <https://github.com/JoaoLopesF/RemoteDebug/pull/56>
- MAJOR CHANGE: Removed ArduinoWebsockets from the sources and used the one from the library manager. There is still "a little problem" with <https://github.com/Links2004/arduinoWebSockets> -- it doens't compile under ESP32 (latest version 2.4.1), so we use the older version 2.3.4. This is a temporary solution, until the problem is fixed.
- 