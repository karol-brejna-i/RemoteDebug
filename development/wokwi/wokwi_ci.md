Based on your new requirements (ESP32, WebSocket Server, External Client testing), the CI workflow requires a significant architectural change.

Since you need to connect an **external client** (running in the GitHub Actions runner) to the **simulated ESP32** (running in Wokwi's cloud), you must establish a network tunnel. This is done using the `[[net.forward]]` configuration in `wokwi.toml`.

Here is the updated proposal.

-----

### **Proposed CI Architecture**

1.  **Environment**: GitHub Actions (Ubuntu Runner).
2.  **Compilation**: `arduino-cli` builds the binary for `esp32`.
3.  **Tunneling**: `wokwi-cli` reads `wokwi.toml`, starts the simulation, and **forwards a local port** (e.g., 8080) on the runner to the simulated ESP32's port (80).
4.  **Testing**: A Python script runs in parallel, connects to `localhost:8080` (which tunnels to the ESP32), and validates the WebSocket responses.

### **Required Files (Copy & Paste)**

You can copy the content below into a file named `esp32_ci_setup.md` or directly implement the files.

````markdown
# ESP32 Wokwi CI with External Client Testing

## 1. Project Configuration (`wokwi.toml`)

This file configures the simulation and, crucially, the network forwarding. 
The `[[net.forward]]` section maps the CI runner's port 8080 to the ESP32's port 80.

```toml
[wokwi]
version = 1
firmware = "build/project.ino.bin"
elf = "build/project.ino.elf"

# Forward CI Runner localhost:8080 -> ESP32 Port 80
[[net.forward]]
from = "localhost:8080"
to = "target:80"
````

## 2\. Hardware Layout (`diagram.json`)

A basic ESP32 setup required for the simulation to boot.

```json
{
  "version": 1,
  "author": "CI User",
  "editor": "wokwi",
  "parts": [
    {
      "type": "board-esp32-devkit-c-v4",
      "id": "esp",
      "top": 0,
      "left": 0,
      "attrs": {}
    }
  ],
  "connections": [
    [ "esp:TX", "$serialMonitor:RX", "", [] ],
    [ "esp:RX", "$serialMonitor:TX", "", [] ]
  ]
}
```

## 3\. The Test Runner Script (`ci_test.sh`)

This script manages the lifecycle: it builds the code, starts the simulator in the **background**, waits for the ESP32 to connect to WiFi, runs the tests, and then cleans up.

```bash
#!/bin/bash
set -e

# --- Configuration ---
SKETCH_NAME="project.ino"
# Common FQBN for ESP32 Dev Module
BOARD_FQBN="esp32:esp32:esp32" 
BUILD_DIR="build"

# --- 1. Setup Environment ---
echo "[CI] Setting up ESP32 Toolchain..."
if ! command -v arduino-cli &> /dev/null; then
    curl -fsSL [https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh](https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh) | sh
    export PATH=$PATH:$PWD/bin
fi

# Install ESP32 Core
if ! arduino-cli core list | grep -q "esp32:esp32"; then
    # Add Espressif URL if not present (simplified for CI)
    arduino-cli config init --additional-urls [https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json](https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json) > /dev/null
    arduino-cli core update-index
    arduino-cli core install esp32:esp32
fi

# --- 2. Build Firmware ---
echo "[CI] Compiling Sketch..."
# ESP32 builds require the partition table and bootloader, handled by --output-dir
arduino-cli compile --fqbn $BOARD_FQBN --output-dir $BUILD_DIR $SKETCH_NAME

# --- 3. Start Simulation in Background ---
echo "[CI] Starting Wokwi Simulation..."
if [ -z "$WOKWI_CLI_TOKEN" ]; then
    echo "Error: WOKWI_CLI_TOKEN is missing."
    exit 1
fi

# Start wokwi-cli and redirect output to a log file so we can parse it
# We do NOT use --timeout here because we control the lifecycle manually
wokwi-cli . > sim.log 2>&1 &
SIM_PID=$!

echo "[CI] Simulator PID: $SIM_PID"

# --- 4. Wait for Server Ready ---
echo "[CI] Waiting for ESP32 to connect to WiFi..."
MAX_RETRIES=60
COUNT=0
SERVER_READY=0

while [ $COUNT -lt $MAX_RETRIES ]; do
    # Check if firmware printed the IP address (Assumes your code prints "IP Address:")
    if grep -q "IP Address" sim.log; then
        SERVER_READY=1
        break
    fi
    # Check if simulation crashed
    if ! kill -0 $SIM_PID 2>/dev/null; then
        echo "[CI] Simulation crashed early!"
        cat sim.log
        exit 1
    fi
    sleep 1
    COUNT=$((COUNT+1))
done

if [ $SERVER_READY -eq 0 ]; then
    echo "[CI] Timeout waiting for WiFi connection."
    cat sim.log
    kill $SIM_PID
    exit 1
fi

echo "[CI] ESP32 is ready. Starting External Client Tests..."

# --- 5. Run External Client Tests ---
# This python script runs on the runner and hits localhost:8080
python3 test_client.py

TEST_EXIT_CODE=$?

# --- 6. Cleanup ---
echo "[CI] Stopping Simulation..."
kill $SIM_PID

# Output the simulation log for debugging
echo "--- Simulation Log ---"
cat sim.log
echo "----------------------"

if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo "SUCCESS: All tests passed."
    exit 0
else
    echo "FAILURE: External client tests failed."
    exit 1
fi
```

## 4\. The External Test Client (`test_client.py`)

This is the script you asked for to conduct request/response tests.

```python
import sys
import websocket # pip install websocket-client
import time

def test_websocket():
    # Connect to localhost:8080 (Forwarded to ESP32 Port 80)
    url = "ws://localhost:8080/ws" 
    
    try:
        print(f"Connecting to {url}...")
        ws = websocket.create_connection(url, timeout=5)
        
        # Test 1: Send Data
        print("Sending: 'ping'")
        ws.send("ping")
        
        # Test 2: Receive Response
        result = ws.recv()
        print(f"Received: '{result}'")
        
        if result == "pong": # Adjust based on your firmware logic
            print("Test 1 Passed: Pong received")
        else:
            print(f"Test 1 Failed: Unexpected response {result}")
            sys.exit(1)
            
        ws.close()
        
    except Exception as e:
        print(f"Connection failed: {e}")
        sys.exit(1)

if __name__ == "__main__":
    # Give the tunnel a moment to stabilize if needed
    time.sleep(2)
    test_websocket()
```

## 5\. GitHub Actions Workflow (`.github/workflows/ci.yml`)

```yaml
name: ESP32 WebSocket CI

on: [push, pull_request]

jobs:
  test-server:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.x'
          
      - name: Install Python Deps
        run: pip install websocket-client

      - name: Setup Node.js (for Wokwi CLI)
        uses: actions/setup-node@v3
        with:
          node-version: 18

      - name: Install Wokwi CLI
        run: npm install -g @wokwi/cli

      - name: Run CI Script
        env:
          WOKWI_CLI_TOKEN: ${{ secrets.WOKWI_CLI_TOKEN }}
        run: |
          chmod +x ci_test.sh
          ./ci_test.sh
```

```

### **Critical Notes for ESP32**
1.  **Network Forwarding Requirement**: The `[[net.forward]]` feature often requires a **Wokwi Club** subscription or a specific license when used in CI, as it consumes significant cloud resources to maintain the active tunnel. Please verify your plan supports this.
2.  **WiFi Credentials**: In your Arduino code, you must connect to the standard Wokwi simulation gateway:
    * **SSID**: `"Wokwi-GUEST"`
    * **Password**: `""` (Empty string)
3.  **Log Parsing**: The `ci_test.sh` relies on `grep -q "IP Address" sim.log`. Ensure your ESP32 firmware does `Serial.println("IP Address: ...")` upon successful connection, otherwise the script will time out.

... [Simulating IoT projects in VS Code | Wokwi, PlatformIO, VS Code](https://www.youtube.com/watch?v=9pTZL934k2s)

This video is relevant because it demonstrates the integration of Wokwi with VS Code and explains configuration concepts like `wokwi.toml` and server simulation which are essential for understanding the proposed CI setup.


http://googleusercontent.com/youtube_content/4
```
