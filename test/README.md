# RemoteDebug Integration Testing Guide

This directory contains **integration tests** for the RemoteDebug library.

> **Note:** These are integration tests that require physical hardware (ESP32/ESP8266).
> They test the library by connecting to a real device over the network via telnet.
> Unit tests are not currently implemented for this library.

## Prerequisites

### Hardware Required
- **ESP32** or **ESP8266** development board
- USB cable for flashing
- WiFi network (device and test machine must be on the same network)

### Software Required
- PlatformIO (CLI or IDE)
- `nc` (netcat) - usually pre-installed on Linux/macOS
- Bash shell

## Quick Start

### 1. Flash the Test Firmware

WiFi credentials are passed via environment variables (works locally and in CI):

```bash
# Set WiFi credentials and flash
WIFI_SSID="YourNetwork" WIFI_PASSWORD="YourPassword" pio run -e integration_test -t upload

# Or export them first (useful for multiple commands)
export WIFI_SSID="YourNetwork"
export WIFI_PASSWORD="YourPassword"
pio run -e integration_test -t upload
pio device monitor
```

**Alternative: Flash any example sketch:**
```bash
pio run -e lolin_d32_pro -t upload --project-dir examples/RemoteDebug_Advanced
```

### 2. Find Device IP Address

After flashing, open the serial monitor to see the device IP:
```bash
pio device monitor -b 115200
```

Look for output like:
```
* WiFi connected. IP address: 192.168.1.100
* RemoteDebug started on port 23
```

### 3. Run Integration Tests

```bash
cd test/integration

# Quick smoke test (~2 min)
./run_tests.sh smoke 192.168.1.100

# Standard regression test (~5 min)
./run_tests.sh basic 192.168.1.100

# Full regression test (~10 min)
./run_tests.sh full 192.168.1.100
```

## Test Suites

| Suite | Duration | Description |
|-------|----------|-------------|
| `smoke` | ~2 min | Essential commands only (h, ?, m, v, d, i, w, e) |
| `basic` | ~5 min | Standard regression (all display/filter commands) |
| `full` | ~10 min | Complete regression (includes edge cases, stability) |
| `commands` | ~5 min | All telnet command tests |
| `levels` | ~2 min | Log level filtering tests |
| `auth` | ~2 min | Authentication tests (requires PASSWORD) |

## Environment Variables

### Build-time Variables (for flashing firmware)

| Variable | Description |
|----------|-------------|
| `WIFI_SSID` | WiFi network name (required for test firmware) |
| `WIFI_PASSWORD` | WiFi password (required for test firmware) |

### Run-time Variables (for running tests)

| Variable | Default | Description |
|----------|---------|-------------|
| `DEVICE_IP` | localhost | IP address of the device |
| `DEVICE_PORT` | 23 | Telnet port |
| `PASSWORD` | (none) | Device password if authentication is enabled |
| `VERBOSE` | 0 | Set to 1 for debug output |
| `TEST_FIRMWARE` | 0 | Set to 1 to enable extended tests (see below) |
| `TIMEOUT` | 3 | Command timeout in seconds |
| `STOP_ON_FAIL` | 0 | Set to 1 to stop on first failure (default: run all tests) |
| `RECONNECT_EACH` | 0 | Set to 1 to reconnect for each command (slower, tests reconnection) |
| `COMMAND_DELAY` | 0.5 | Delay between commands in seconds (device has 500ms duplicate filter) |

### Connection Modes

By default, the test script uses a **persistent connection** for speed. All commands are sent through a single telnet session.

**Persistent connection (`RECONNECT_EACH=0`, default):**
- Fast (~30 seconds for full suite)
- Tests command sequences and state
- Single connection for all tests

**Reconnect mode (`RECONNECT_EACH=1`):**
- Slower (~3 minutes for full suite)
- Each command opens a new connection
- Tests reconnection handling
- Each test starts with fresh state

### Standard vs Extended Tests

By default, tests run against **any firmware** that uses RemoteDebug. This means you can run the test suite against your own application to verify RemoteDebug integration works correctly.

**Standard tests (`TEST_FIRMWARE=0`, default):**
- Test built-in RemoteDebug commands (`help`, `m`, `v`, `d`, `i`, `w`, `e`, etc.)
- Work with any firmware that includes RemoteDebug
- Verify your application's RemoteDebug integration

**Extended tests (`TEST_FIRMWARE=1`):**
- Include additional tests for custom commands only available in `test_firmware.ino`
- Commands like `ping`, `test_echo`, `test_all_levels`, `test_status`
- Require flashing the dedicated test firmware (`pio run -e integration_test -t upload`)
- Used for comprehensive library regression testing

| Scenario | Command |
|----------|---------|
| Test your own app | `./run_tests.sh smoke 192.168.1.100` |
| Test with dedicated firmware | `TEST_FIRMWARE=1 ./run_tests.sh full 192.168.1.100` |

When `TEST_FIRMWARE=0`, tests requiring custom commands are skipped with `[SKIP]` in the output.

## CI/CD Integration

For GitHub Actions, add secrets and use them in your workflow:

```yaml
# .github/workflows/integration-test.yml
jobs:
  test:
    runs-on: self-hosted  # Requires ESP32 connected to runner
    steps:
      - uses: actions/checkout@v4
      
      - name: Flash test firmware
        env:
          WIFI_SSID: ${{ secrets.WIFI_SSID }}
          WIFI_PASSWORD: ${{ secrets.WIFI_PASSWORD }}
        run: pio run -e integration_test -t upload
      
      - name: Run integration tests
        env:
          DEVICE_IP: ${{ secrets.DEVICE_IP }}
        run: ./test/integration/run_tests.sh full
```

**Required GitHub Secrets:**
- `WIFI_SSID` - WiFi network name
- `WIFI_PASSWORD` - WiFi password  
- `DEVICE_IP` - IP address of the test device

## Examples

```bash
# Using environment variables
DEVICE_IP=192.168.1.100 ./run_tests.sh full

# With verbose output
VERBOSE=1 ./run_tests.sh smoke 192.168.1.100

# With password authentication
PASSWORD=mysecret ./run_tests.sh auth 192.168.1.100

# Using test firmware for extended tests
TEST_FIRMWARE=1 ./run_tests.sh full 192.168.1.100
```

## Manual Testing with Telnet

You can also test manually:
```bash
# Connect to device
nc 192.168.1.100 23

# Or use telnet
telnet 192.168.1.100 23
```

Common commands to try:
- `h` or `?` - Show help
- `m` - Show free memory
- `v`, `d`, `i`, `w`, `e` - Set debug level
- `c` - Toggle colors
- `t` - Toggle timestamps
- `p` - Toggle profiler
- `q` - Quit connection

## Directory Structure

```
test/
├── README.md           # This file
├── build/
│   └── build_test.ino  # Build verification sketch
└── integration/
    ├── run_tests.sh       # Integration test script
    └── test_firmware.ino  # Dedicated test firmware with test commands
```

## Test Firmware

For extended testing, flash the dedicated test firmware:
```bash
pio run -e integration_test -t upload
```

### LED Status Indicators

The test firmware uses the built-in LED to indicate device status:

| LED Pattern | Meaning |
|-------------|---------|
| Fast blink (100ms) | Connecting to WiFi |
| Slow blink (1s) | WiFi connection failed |
| Solid ON | Connected and running |
| Brief OFF pulse | Heartbeat (every 5 seconds) |

### Test Commands

This firmware adds special test commands:
- `test_all_levels` - Output messages at all debug levels
- `test_flood` - Send rapid messages
- `test_long` - Send long messages
- `test_special` - Test special characters
- `test_colors` - Test color output
- `test_status` - Show device status (uptime, heap, IP)
- `test_echo <msg>` - Echo back a message
- `ping` - Simple connectivity check (responds with "pong")
- `test_help` - Show all test commands

**API Test Commands** (for verifying RemoteDebug API methods):
- `test_last_cmd` - Verify `getLastCommand()` API (TC-API-006)
- `test_clear_cmd` - Verify `clearLastCommand()` API (TC-API-007)
- `test_connected` - Verify `isConnected()` API (TC-API-009)
- `test_silence` - Verify `isSilence()` API (TC-API-014)
- `test_callback` - Verify callback mechanism works (TC-API-008)

## Troubleshooting

### Device not reachable
1. Check device is powered and connected to WiFi
2. Verify IP address in serial monitor
3. Ensure test machine is on same network
4. Check firewall allows port 23

### Tests timing out
- Increase timeout: `TIMEOUT=10 ./run_tests.sh basic 192.168.1.100`
- Check network latency
- Verify only one telnet client is connected

### Connection refused
- RemoteDebug allows only one client at a time
- Close any existing telnet sessions
- Wait a few seconds and retry

## Related Documentation

- [TESTING_STRATEGY.md](../development/TESTING_STRATEGY.md) - Overall testing approach
- [TEST_CASES.md](../development/TEST_CASES.md) - Detailed test case specifications
