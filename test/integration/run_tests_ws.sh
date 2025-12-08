#!/bin/bash
#------------------------------------------------------------------------------
# RemoteDebug WebSocket Integration Test Suite
#------------------------------------------------------------------------------
# Tests RemoteDebug functionality over WebSocket connection (port 8232)
#
# Requirements:
#   - Device running test_firmware.ino built with integration_test_ws environment
#   - Python websocket-client (pip install websocket-client) OR websocat binary
#   - Device IP address
#
# Usage:
#   ./run_tests_ws.sh [test_suite] [device_ip]
#
# Examples:
#   ./run_tests_ws.sh smoke 192.168.1.100
#   DEVICE_IP=192.168.1.100 ./run_tests_ws.sh full
#   VERBOSE=1 ./run_tests_ws.sh basic 192.168.1.100
#
# Environment Variables:
#   DEVICE_IP      - Device IP address (required)
#   DEVICE_PORT    - WebSocket port (default: 8232)
#   TIMEOUT        - Command timeout in seconds (default: 5)
#   VERBOSE        - Set to 1 for debug output
#   TEST_FIRMWARE  - Set to 1 for extended tests (default: 1 for WS tests)
#   COMMAND_DELAY  - Delay between commands in seconds (default: 0.5)
#
#------------------------------------------------------------------------------

set -o pipefail

# Configuration
DEVICE_IP="${DEVICE_IP:-${2:-}}"
DEVICE_PORT="${DEVICE_PORT:-8232}"
TIMEOUT="${TIMEOUT:-5}"
VERBOSE="${VERBOSE:-0}"
TEST_FIRMWARE="${TEST_FIRMWARE:-1}"  # WebSocket tests typically use test firmware
COMMAND_DELAY="${COMMAND_DELAY:-0.5}"

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

# Timing
START_TIME=$(date +%s)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# WebSocket client detection
WS_CLIENT=""
WS_CLIENT_TYPE=""

#------------------------------------------------------------------------------
# Utility Functions
#------------------------------------------------------------------------------

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_pass() { echo -e "${GREEN}[PASS]${NC} $1"; ((TESTS_PASSED++)); }
log_fail() { echo -e "${RED}[FAIL]${NC} $1"; ((TESTS_FAILED++)); }
log_skip() { echo -e "${YELLOW}[SKIP]${NC} $1"; ((TESTS_SKIPPED++)); }
log_section() { echo -e "\n${BOLD}=== $1 ===${NC}"; }
log_verbose() { [[ "$VERBOSE" == "1" ]] && echo -e "${CYAN}[DEBUG]${NC} $1"; }

detect_ws_client() {
    # Try websocat first (faster, more reliable)
    if command -v websocat &>/dev/null; then
        WS_CLIENT="websocat"
        WS_CLIENT_TYPE="websocat"
        log_verbose "Using websocat for WebSocket connections"
        return 0
    fi
    
    # Try Python websockets
    if python3 -c "import websockets" 2>/dev/null; then
        WS_CLIENT="python3"
        WS_CLIENT_TYPE="python"
        log_verbose "Using Python websockets for WebSocket connections"
        return 0
    fi
    
    # Try Python websocket-client
    if python3 -c "import websocket" 2>/dev/null; then
        WS_CLIENT="python3"
        WS_CLIENT_TYPE="python-sync"
        log_verbose "Using Python websocket-client for WebSocket connections"
        return 0
    fi
    
    echo -e "${RED}[ERROR]${NC} No WebSocket client found!"
    echo ""
    echo "Please install one of the following:"
    echo "  1. Python websocket-client (recommended):"
    echo "     pip install websocket-client"
    echo "     # or: pip install --user websocket-client --break-system-packages"
    echo ""
    echo "  2. websocat binary:"
    echo "     Download from: https://github.com/vi/websocat/releases"
    return 1
}

# Send command via WebSocket and capture response
send_command() {
    local cmd="$1"
    local response=""
    
    log_verbose "Sending: '$cmd'"
    
    case "$WS_CLIENT_TYPE" in
        websocat)
            # Use websocat with timeout
            response=$(echo "$cmd" | timeout "$TIMEOUT" websocat -t -1 "ws://$DEVICE_IP:$DEVICE_PORT/" 2>/dev/null || true)
            ;;
        python|python-sync)
            # Use Python script for WebSocket
            # Read multiple responses since command echo and actual response come separately
            response=$(timeout "$TIMEOUT" python3 -c "
import sys
try:
    from websocket import create_connection
    ws = create_connection('ws://$DEVICE_IP:$DEVICE_PORT/', timeout=$TIMEOUT)
    # Read and discard welcome message
    welcome = ws.recv()
    # Send command
    ws.send('$cmd')
    # Read multiple responses (command echo + actual response may be separate)
    import time
    time.sleep(0.3)
    ws.settimeout(0.5)
    responses = []
    for _ in range(5):  # Read up to 5 messages
        try:
            r = ws.recv()
            responses.append(r)
        except:
            break
    # Print all responses
    for r in responses:
        print(r)
    ws.close()
except Exception as e:
    print(f'Error: {e}', file=sys.stderr)
" 2>/dev/null || true)
            ;;
    esac
    
    # Wait before next command
    sleep "$COMMAND_DELAY"
    
    log_verbose "Response length: ${#response} bytes"
    [[ "$VERBOSE" == "1" ]] && [[ ${#response} -lt 500 ]] && log_verbose "Response: $response"
    
    echo "$response"
}

# Test WebSocket connection
test_ws_connection() {
    log_verbose "Testing WebSocket connection to ws://$DEVICE_IP:$DEVICE_PORT/"
    
    local response=""
    
    case "$WS_CLIENT_TYPE" in
        websocat)
            response=$(timeout "$TIMEOUT" websocat -t -1 "ws://$DEVICE_IP:$DEVICE_PORT/" 2>/dev/null <<< "" || true)
            ;;
        python|python-sync)
            response=$(timeout "$TIMEOUT" python3 -c "
import sys
try:
    from websocket import create_connection
    ws = create_connection('ws://$DEVICE_IP:$DEVICE_PORT/', timeout=$TIMEOUT)
    welcome = ws.recv()
    print(welcome)
    ws.close()
except Exception as e:
    sys.exit(1)
" 2>/dev/null || true)
            ;;
    esac
    
    if [[ -n "$response" ]]; then
        return 0
    else
        return 1
    fi
}

wait_for_device() {
    log_info "Waiting for device at ws://$DEVICE_IP:$DEVICE_PORT/..."
    
    local attempts=0
    local max_attempts=10
    
    while [[ $attempts -lt $max_attempts ]]; do
        if test_ws_connection; then
            log_info "Device is ready!"
            return 0
        fi
        ((attempts++))
        log_verbose "Connection attempt $attempts/$max_attempts failed, retrying..."
        sleep 1
    done
    
    log_fail "Could not connect to device after $max_attempts attempts"
    return 1
}

#------------------------------------------------------------------------------
# Test Cases
#------------------------------------------------------------------------------

test_connection() {
    log_section "1. WebSocket Connection Tests (TC-WS-CON)"
    
    # TC-WS-CON-001: WebSocket handshake
    if test_ws_connection; then
        log_pass "TC-WS-CON-001: WebSocket handshake successful"
    else
        log_fail "TC-WS-CON-001: WebSocket handshake failed"
        return 1
    fi
    
    # TC-WS-CON-002: Welcome message
    # Note: WebSocket sends $app: protocol messages, not human-readable welcome like telnet
    local response
    case "$WS_CLIENT_TYPE" in
        websocat)
            response=$(timeout "$TIMEOUT" websocat -t -1 "ws://$DEVICE_IP:$DEVICE_PORT/" 2>/dev/null <<< "" || true)
            ;;
        python|python-sync)
            response=$(timeout "$TIMEOUT" python3 -c "
from websocket import create_connection
ws = create_connection('ws://$DEVICE_IP:$DEVICE_PORT/', timeout=$TIMEOUT)
print(ws.recv())
ws.close()
" 2>/dev/null || true)
            ;;
    esac
    
    # WebSocket welcome can be either $app: protocol message or human-readable
    if echo "$response" | grep -qiE 'Remote.*debug|RemoteDebug|Welcome|\$app:'; then
        log_pass "TC-WS-CON-002: Welcome message received"
    else
        log_fail "TC-WS-CON-002: No welcome message"
        log_verbose "Response: $response"
    fi
}

test_commands() {
    log_section "2. Command Tests over WebSocket (TC-WS-CMD)"
    
    local response
    
    # TC-WS-CMD-001: Help command
    response=$(send_command "h")
    if echo "$response" | grep -qiE "help|command"; then
        log_pass "TC-WS-CMD-001: Help command (h)"
    else
        log_fail "TC-WS-CMD-001: Help command failed"
    fi
    
    # TC-WS-CMD-002: Memory command
    response=$(send_command "m")
    if echo "$response" | grep -qiE "memory|free|heap|[0-9]"; then
        log_pass "TC-WS-CMD-002: Memory command (m)"
    else
        log_fail "TC-WS-CMD-002: Memory command failed"
    fi
    
    # TC-WS-CMD-003: Debug level verbose
    response=$(send_command "v")
    if echo "$response" | grep -qiE "verbose|level"; then
        log_pass "TC-WS-CMD-003: Set verbose level (v)"
    else
        log_fail "TC-WS-CMD-003: Set verbose level failed"
    fi
    
    # TC-WS-CMD-004: Toggle time
    response=$(send_command "t")
    if echo "$response" | grep -qiE "time|show"; then
        log_pass "TC-WS-CMD-004: Toggle time (t)"
    else
        log_fail "TC-WS-CMD-004: Toggle time failed"
    fi
    
    # TC-WS-CMD-005: Toggle colors
    response=$(send_command "c")
    if echo "$response" | grep -qiE "color"; then
        log_pass "TC-WS-CMD-005: Toggle colors (c)"
    else
        log_fail "TC-WS-CMD-005: Toggle colors failed"
    fi
}

test_firmware_commands() {
    log_section "3. Test Firmware Commands over WebSocket (TC-WS-FW)"
    
    if [[ "$TEST_FIRMWARE" != "1" ]]; then
        log_skip "TC-WS-FW-*: Skipped (TEST_FIRMWARE not set)"
        return
    fi
    
    local response
    
    # Ensure verbose level for INFO messages
    send_command "v" >/dev/null
    
    # TC-WS-FW-001: Ping/Pong
    response=$(send_command "ping")
    if echo "$response" | grep -qiE "pong"; then
        log_pass "TC-WS-FW-001: ping -> pong"
    else
        log_fail "TC-WS-FW-001: ping -> pong failed"
        log_verbose "Response: $response"
    fi
    
    # TC-WS-FW-002: Echo command
    response=$(send_command "test_echo WebSocket_Test")
    if echo "$response" | grep -qiE "WebSocket_Test"; then
        log_pass "TC-WS-FW-002: test_echo works"
    else
        log_fail "TC-WS-FW-002: test_echo failed"
    fi
    
    # TC-WS-FW-003: Status command
    response=$(send_command "test_status")
    if echo "$response" | grep -qiE "uptime|heap|status"; then
        log_pass "TC-WS-FW-003: test_status works"
    else
        log_fail "TC-WS-FW-003: test_status failed"
    fi
    
    # TC-WS-FW-004: API test - isConnected
    response=$(send_command "test_connected")
    if echo "$response" | grep -qiE "CONNECTED:1"; then
        log_pass "TC-WS-FW-004: isConnected() returns true"
    else
        log_fail "TC-WS-FW-004: isConnected() failed"
    fi
}

test_stability() {
    log_section "4. WebSocket Stability Tests (TC-WS-STAB)"
    
    local response
    
    # TC-WS-STAB-001: Multiple commands in sequence
    log_info "Sending command sequence..."
    for cmd in "v" "m" "t" "t" "c" "c"; do
        send_command "$cmd" >/dev/null
    done
    
    response=$(send_command "m")
    if echo "$response" | grep -qiE "memory|heap|[0-9]"; then
        log_pass "TC-WS-STAB-001: Command sequence stable"
    else
        log_fail "TC-WS-STAB-001: Device unresponsive after sequence"
    fi
    
    # TC-WS-STAB-002: Long command
    local long_cmd
    long_cmd=$(printf 'x%.0s' {1..200})
    send_command "$long_cmd" >/dev/null
    
    response=$(send_command "m")
    if echo "$response" | grep -qiE "memory|heap|[0-9]"; then
        log_pass "TC-WS-STAB-002: Handles long commands"
    else
        log_fail "TC-WS-STAB-002: Crashed on long command"
    fi
}

#------------------------------------------------------------------------------
# Test Suites
#------------------------------------------------------------------------------

run_smoke_tests() {
    log_info ""
    log_info "========================================"
    log_info "  RemoteDebug WebSocket SMOKE Tests"
    log_info "  Device: ws://$DEVICE_IP:$DEVICE_PORT/"
    log_info "========================================"
    
    test_connection || return 1
    
    # Quick command test
    local response
    response=$(send_command "m")
    if echo "$response" | grep -qiE "memory|heap|[0-9]"; then
        log_pass "Smoke: Commands work over WebSocket"
    else
        log_fail "Smoke: Commands not working"
    fi
}

run_basic_tests() {
    log_info ""
    log_info "========================================"
    log_info "  RemoteDebug WebSocket BASIC Tests"
    log_info "  Device: ws://$DEVICE_IP:$DEVICE_PORT/"
    log_info "========================================"
    
    test_connection || return 1
    test_commands
}

run_full_tests() {
    log_info ""
    log_info "========================================"
    log_info "  RemoteDebug WebSocket FULL Tests"
    log_info "  Device: ws://$DEVICE_IP:$DEVICE_PORT/"
    log_info "  Test Firmware: $TEST_FIRMWARE"
    log_info "========================================"
    
    test_connection || return 1
    test_commands
    test_firmware_commands
    test_stability
}

#------------------------------------------------------------------------------
# Results Summary
#------------------------------------------------------------------------------

print_summary() {
    local end_time
    end_time=$(date +%s)
    local duration=$((end_time - START_TIME))
    
    echo ""
    echo "========================================"
    echo "  WebSocket Test Results"
    echo "========================================"
    echo -e "  ${GREEN}Passed:${NC}  $TESTS_PASSED"
    echo -e "  ${RED}Failed:${NC}  $TESTS_FAILED"
    echo -e "  ${YELLOW}Skipped:${NC} $TESTS_SKIPPED"
    echo "  ----------------------"
    echo "  Total:   $((TESTS_PASSED + TESTS_FAILED + TESTS_SKIPPED))"
    echo "  Time:    ${duration}s"
    echo "========================================"
    
    if [[ $TESTS_FAILED -gt 0 ]]; then
        echo -e "\n${RED}FAILED${NC} - $TESTS_FAILED test(s) failed"
        return 1
    else
        echo -e "\n${GREEN}SUCCESS${NC} - All tests passed"
        return 0
    fi
}

#------------------------------------------------------------------------------
# Help
#------------------------------------------------------------------------------

show_help() {
    echo "RemoteDebug WebSocket Integration Test Suite"
    echo ""
    echo "Usage: $0 [test_suite] [device_ip]"
    echo ""
    echo "Test Suites:"
    echo "  smoke  - Quick connectivity test"
    echo "  basic  - Standard command tests"
    echo "  full   - Complete test suite"
    echo ""
    echo "Environment Variables:"
    echo "  DEVICE_IP      - Device IP (required)"
    echo "  DEVICE_PORT    - WebSocket port (default: 8232)"
    echo "  TIMEOUT        - Command timeout in seconds (default: 5)"
    echo "  VERBOSE        - Set to 1 for debug output"
    echo "  TEST_FIRMWARE  - Set to 1 if using test firmware (default: 1)"
    echo "  COMMAND_DELAY  - Delay between commands (default: 0.5)"
    echo ""
    echo "Requirements:"
    echo "  - Device must be flashed with: pio run -e integration_test_ws -t upload"
    echo "  - Install: pip install websocket-client"
    echo "    OR Python: pip install websocket-client"
    echo ""
    echo "Examples:"
    echo "  $0 smoke 192.168.1.100"
    echo "  DEVICE_IP=192.168.1.100 $0 full"
    echo "  VERBOSE=1 $0 basic 192.168.1.100"
}

#------------------------------------------------------------------------------
# Main
#------------------------------------------------------------------------------

main() {
    local test_suite="${1:-full}"
    
    # Check for help
    if [[ "$test_suite" == "-h" ]] || [[ "$test_suite" == "--help" ]]; then
        show_help
        exit 0
    fi
    
    # Validate device IP
    if [[ -z "$DEVICE_IP" ]]; then
        echo -e "${RED}[ERROR]${NC} Device IP is required"
        echo ""
        show_help
        exit 1
    fi
    
    # Detect WebSocket client
    detect_ws_client || exit 1
    
    case "$test_suite" in
        smoke)
            wait_for_device || exit 1
            run_smoke_tests
            ;;
        basic)
            wait_for_device || exit 1
            run_basic_tests
            ;;
        full)
            wait_for_device || exit 1
            run_full_tests
            ;;
        *)
            echo -e "${RED}[ERROR]${NC} Unknown test suite: $test_suite"
            show_help
            exit 1
            ;;
    esac
    
    print_summary
}

main "$@"
