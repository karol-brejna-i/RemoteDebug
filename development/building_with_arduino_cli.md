# Building RemoteDebug with Arduino IDE 2.0 and Arduino CLI

This guide explains how to build and verify the RemoteDebug library and its examples using Arduino IDE 2.0 and the Arduino CLI. This ensures compatibility for both GUI and automated/CI workflows.

---

## 1. Prerequisites

- **Arduino IDE 2.0** installed (for GUI builds)
- **Arduino CLI** installed ([official instructions](https://arduino.github.io/arduino-cli/latest/installation/))
- Supported board (ESP8266 or ESP32)
- USB cable for flashing (if testing on hardware)

---

## 2. Install Board Packages

### ESP32:
```sh
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

### ESP8266:
```sh
arduino-cli core update-index
arduino-cli core install esp8266:esp8266
```

---

## 3. Install Library Dependencies

The main dependency is `links2004/WebSockets` (should be auto-installed via library.json, but can be installed manually):

```sh
arduino-cli lib install "WebSockets@2.7.1"
```

---

## 4. Build Examples via Arduino CLI

### List available boards:
```sh
arduino-cli board listall
```

### Example: Build for ESP32 Dev Module
```sh
arduino-cli compile --fqbn esp32:esp32:esp32 examples/Simple
```

### Example: Build for ESP8266 D1 Mini
```sh
arduino-cli compile --fqbn esp8266:esp8266:d1_mini examples/Simple
```

### Build all examples (ESP32):
```sh
for ex in examples/*; do
  arduino-cli compile --fqbn esp32:esp32:esp32 "$ex"
done
```

---

## 5. Upload to Device (Optional)

Plug in your device and find its port (e.g., `/dev/ttyUSB0`):

```sh
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 examples/Simple
```

---

## 6. Build in Arduino IDE 2.0 GUI

1. Open Arduino IDE 2.0
2. Open the desired example sketch (e.g., `examples/Simple/Simple.ino`)
3. Select the correct board and port
4. Click the checkmark (Verify) or right arrow (Upload)

---

## 7. Troubleshooting

- If libraries are not found, use `arduino-cli lib install` to add them.
- For ESP32-C6 or other special boards, ensure the correct core is installed and note WebSockets limitations.
- If you see build errors, check for board core/library version mismatches.

---

## 8. CI Integration (Optional)

You can add a GitHub Actions job to build all examples using Arduino CLI for regression testing.

---

## References
- [Arduino CLI Documentation](https://arduino.github.io/arduino-cli/latest/)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [ESP8266 Arduino Core](https://github.com/esp8266/Arduino)
