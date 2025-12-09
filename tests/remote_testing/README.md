# Remote Integration Testing with PlatformIO Remote

This guide shows how to run the existing integration tests against a **physically connected device** when the CI runner cannot access USB directly. PlatformIO Remote ships the firmware and executes commands through a long-lived agent that sits next to the hardware.

## What you get
- Flash firmware on a remote ESP32/ESP8266 over the internet/LAN.
- Read serial logs remotely (to discover the device IP, etc.).
- Run the telnet/WebSocket integration tests from CI using the existing scripts under `test/integration`.

## Prerequisites
- A host machine with the board attached over USB ("remote lab host").
- PlatformIO Core 6+ installed on both the lab host and CI runner (`pipx install platformio`).
- A PlatformIO account and a token for non-interactive login (store in `PLATFORMIO_AUTH_TOKEN`).
- The lab host can reach the device over WiFi and has outbound internet to talk to PlatformIO Cloud.

## 1) Prepare the lab host (one-time)
```bash
# Install PlatformIO Core
pipx install platformio

# Authenticate once (stores credentials locally)
pio account login --token "$PLATFORMIO_AUTH_TOKEN"

# Start the agent; give it a friendly name
pio remote agent start --name lab-esp32
```
Keep the agent running (systemd service or tmux). Verify it shows up:
```bash
pio remote agent list
```

## 2) Build and flash remotely
Use the existing envs from `platformio.ini` (e.g., `integration_test` for telnet, `integration_test_ws` for WebSocket).
```bash
# From CI or your laptop (no USB needed on the caller)
pio remote run --agent lab-esp32 -e integration_test -t upload \
  -e integration_test_ws -t upload \
  --upload-port auto \
  --project-dir . \
  --json-output <<'EOF'
EOF
```
WiFi credentials are forwarded as env vars (same as local):
```bash
WIFI_SSID="YourNetwork" WIFI_PASSWORD="YourPass" \
pio remote run --agent lab-esp32 -e integration_test -t upload
```

## 3) Grab the device IP from remote serial
After flashing, read the serial output through the agent to learn the IP the firmware picked up:
```bash
pio remote device monitor --agent lab-esp32 --baud 115200 --timeout 30 | tee device.log
# Extract IP (example pattern from test firmware)
grep -oE "IP address: [0-9.]+" device.log | awk '{print $3}'
```
Save the discovered IP as `DEVICE_IP` for the test step.

## 4) Run the integration tests through the agent
Run the existing scripts on the lab host so they execute near the device:
```bash
# Telnet tests (existing script)
pio remote exec --agent lab-esp32 --project-dir . \
  "TEST_FIRMWARE=1 DEVICE_IP=${DEVICE_IP} ./test/integration/run_tests.sh full"

# WebSocket tests
pio remote exec --agent lab-esp32 --project-dir . \
  "TEST_FIRMWARE=1 DEVICE_IP=${DEVICE_IP} ./test/integration/run_tests_ws.sh basic"
```
Notes:
- `--project-dir .` uploads the current repo to the agent and runs commands in that path.
- Use `TEST_FIRMWARE=1` to enable extended cases provided by `test_firmware.ino`.
- Adjust the suite name (`smoke|basic|full`) to control duration.

## 5) GitHub Actions example (remote hardware, cloud runner)
```yaml
name: remote-integration
on: workflow_dispatch
jobs:
  remote-it:
    runs-on: ubuntu-latest
    env:
      AGENT_NAME: lab-esp32
    steps:
      - uses: actions/checkout@v4

      - name: Install PlatformIO Core
        run: pipx install platformio

      - name: Authenticate to PlatformIO Cloud
        env:
          PLATFORMIO_AUTH_TOKEN: ${{ secrets.PLATFORMIO_AUTH_TOKEN }}
        run: pio account login --token "$PLATFORMIO_AUTH_TOKEN"

      - name: Flash test firmware via agent
        env:
          WIFI_SSID: ${{ secrets.WIFI_SSID }}
          WIFI_PASSWORD: ${{ secrets.WIFI_PASSWORD }}
        run: |
          pio remote run --agent "$AGENT_NAME" -e integration_test -t upload

      - name: Capture IP from serial
        run: |
          pio remote device monitor --agent "$AGENT_NAME" --baud 115200 --timeout 30 | tee device.log
          ip=$(grep -oE "IP address: [0-9.]+" device.log | awk '{print $3}')
          if [ -z "$ip" ]; then echo "Device IP not found" && exit 1; fi
          echo "DEVICE_IP=$ip" >> $GITHUB_ENV

      - name: Run telnet integration tests on the agent
        env:
          DEVICE_IP: ${{ env.DEVICE_IP }}
        run: |
          pio remote exec --agent "$AGENT_NAME" --project-dir . \
            "DEVICE_IP=${DEVICE_IP} TEST_FIRMWARE=1 ./test/integration/run_tests.sh smoke"
```
Secrets to configure:
- `PLATFORMIO_AUTH_TOKEN` — token from `pio account token --create`.
- `WIFI_SSID`, `WIFI_PASSWORD` — WiFi credentials for the test firmware.

## Operational tips
- Keep one agent per board to avoid port conflicts; name them clearly (`lab-esp32`, `lab-esp32-ws`).
- If flashing fails, check the agent logs; USB may have been grabbed by another process.
- You can inspect files on the agent workspace with `pio remote ls`, `pio remote cat <file>` after a run.
- For quicker iterations, prefer `smoke` suite; use `full` before releases.
