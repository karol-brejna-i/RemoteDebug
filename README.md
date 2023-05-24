# RemoteDebug Library

A library for ESP2866 and ESP32 for debuging projects over WiFi.

RemoteDebug sets up a TCP/IP server, that you connect to using telnet or using a dedicated web app.

This project is a fork of not supported (as it seems, last update on May 9, 2019) [RemoteDebug](https://github.com/JoaoLopesF/RemoteDebug) library by Joao Lopes. Here we have some critical bug fixes and feature improvements.

See [old project readme](./old_README.md) for more background.

## Features

The same as the original library.

### Changes from the original library

* Included changes from the following PRs (not included in the original library ATTOW):
  * <https://github.com/JoaoLopesF/RemoteDebug/pull/73>
  * <https://github.com/JoaoLopesF/RemoteDebug/pull/56>
* MAJOR CHANGE: Removed ArduinoWebsockets from the sources and used the one from the library manager. There is still "a little problem" with <https://github.com/Links2004/arduinoWebSockets> -- it doens't compile under ESP32 (latest version 2.4.1), so we use the older version 2.3.4. This is a temporary solution, until the problem is fixed.
