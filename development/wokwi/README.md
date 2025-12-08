# Using Wokwi for Integration Tests

[Wokwi](https://wokwi.com) is an online simulator for embedded systems (ESP32, ESP8266, Arduino, etc.) that can be used for integration testing of the RemoteDebug library without physical hardware.

## Overview

Wokwi allows you to:
- Simulate ESP32/ESP8266 boards in the browser or CLI
- Run firmware without physical hardware
- Simulate WiFi connectivity and network interactions
- Automate testing via the Wokwi CLI

## Setup Methods

### 1. Browser-Based Testing (Manual)

1. Go to [wokwi.com](https://wokwi.com)
2. Create a new ESP32 project
3. Copy your sketch code into the editor
4. Add required library files or use the Library Manager
5. Click "Start Simulation"

### 2. Wokwi CLI (Automated Testing)

Install the CLI:

```bash
# Using npm
npm install -g @wokwi/wokwi-cli

# Or download binary from GitHub releases
# https://github.com/wokwi/wokwi-cli/releases
```

## Project Configuration

### wokwi.toml

Create a `wokwi.toml` file in your project root:

```toml
[wokwi]
version = 1
firmware = ".pio/build/esp32dev/firmware.bin"
elf = ".pio/build/esp32dev/firmware.elf"

[[chip]]
name = "esp32"

[network]
# Forward ESP32's port 23 (telnet) to localhost:2323
forward = ["localhost:2323:23"]
```

### diagram.json

Create a `diagram.json` to define the virtual hardware:

```json
{
  "version": 1,
  "author": "RemoteDebug",
  "editor": "wokwi",
  "parts": [
    {
      "type": "wokwi-esp32-devkit-v1",
      "id": "esp",
      "top": 0,
      "left": 0
    }
  ],
  "connections": []
}
```

## Uploading Firmware

### Using PlatformIO Build Output

```bash
# Build firmware first
pio run -e esp32dev

# Run with Wokwi CLI
wokwi-cli --timeout 30000 .
```

### Using Arduino CLI Build

```bash
# Compile
arduino-cli compile --fqbn esp32:esp32:esp32 ./test/integration/

# The .bin file will be in the build output directory
```

## Connecting to Simulated Device

### WiFi Simulation

Wokwi simulates WiFi with internet access. Your firmware connects using:

```cpp
WiFi.begin("Wokwi-GUEST", "");  // Wokwi's open network (no password)
```

### Accessing Telnet/Network Services

When running locally with Wokwi CLI, use **port forwarding** configured in `wokwi.toml`:

```toml
[network]
forward = ["localhost:2323:23"]
```

Then connect from your host machine:

```bash
telnet localhost 2323
```

### Virtual Serial Monitor

The Wokwi CLI outputs serial to stdout:

```bash
wokwi-cli . 2>&1 | tee simulation.log
```

## Integration Test Workflow

1. **Build firmware** with test configuration:
   ```bash
   pio run -e esp32dev
   ```

2. **Start simulation** with network forwarding:
   ```bash
   wokwi-cli --timeout 60000 .
   ```

3. **Run test scripts** against the simulated device:
   ```bash
   # Wait for device to boot and connect
   sleep 10
   
   # Connect and send debug commands
   echo "help" | nc localhost 2323
   ```

4. **Validate output** from serial and telnet responses

## Example Test Script

```bash
#!/bin/bash
# integration/wokwi_test.sh

set -e

# Start Wokwi in background
wokwi-cli --timeout 120000 . &
WOKWI_PID=$!

# Wait for boot
sleep 15

# Test telnet connection
RESPONSE=$(echo "help" | timeout 5 nc localhost 2323)

if [[ "$RESPONSE" == *"Commands"* ]]; then
    echo "✅ Telnet test passed"
    EXIT_CODE=0
else
    echo "❌ Telnet test failed"
    EXIT_CODE=1
fi

kill $WOKWI_PID 2>/dev/null || true
exit $EXIT_CODE
```

## GitHub Actions Integration

Add Wokwi tests to your CI pipeline:

```yaml
name: Wokwi Integration Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Setup PlatformIO
        uses: platformio/setup-platformio-action@v1
        
      - name: Build Firmware
        run: pio run -e esp32dev
        
      - name: Run Wokwi Tests
        uses: wokwi/wokwi-ci-action@v1
        with:
          token: ${{ secrets.WOKWI_CLI_TOKEN }}
          timeout: 60000
          scenario: test/scenarios/basic.yaml
```

### Getting a Wokwi CI Token

1. Sign up at [wokwi.com](https://wokwi.com)
2. Go to your account settings
3. Generate a CI token
4. Add it as `WOKWI_CLI_TOKEN` in your repository secrets

## Test Scenarios (Optional)

Create scenario files for automated testing in `test/scenarios/`:

```yaml
# test/scenarios/basic.yaml
name: Basic RemoteDebug Test
steps:
  - wait: 10000  # Wait for WiFi connection
  - expect-serial: "RemoteDebug started"
  - connect:
      host: localhost
      port: 2323
  - send: "help\n"
  - expect: "Commands available"
```

## Limitations

- **Network latency**: Simulated but not identical to real hardware
- **Hardware peripherals**: Some may not be fully supported
- **CI usage**: Requires Wokwi account/token
- **WebSocket**: May behave differently than on real ESP32
- **Timing**: Some timing-sensitive code may behave differently

## Resources

- [Wokwi Documentation](https://docs.wokwi.com/)
- [Wokwi CLI GitHub](https://github.com/wokwi/wokwi-cli)
- [Wokwi ESP32 Simulation](https://docs.wokwi.com/guides/esp32)
- [Wokwi CI/CD Integration](https://docs.wokwi.com/guides/ci)
