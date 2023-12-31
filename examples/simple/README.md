# Simple example

This is a very basic example of RemoteDebug library usage.

This example connects to WiFi (remember to set up SSID and password in the code), and initializes the RemoteDebug library.

After that, the following logic is executed:

- Each second, the built-in led is blinked and a message is sent to RemoteDebug (in verbose level)
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
