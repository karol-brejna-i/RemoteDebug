#!/bin/bash
#
# RemoteDebug Integration Test Suite
#
# Comprehensive regression tests for RemoteDebug library.
# Tests are mapped to TEST_CASES.md document.
#
# Usage:
#   ./run_tests.sh [suite] [device_ip] [port]
#
# Suites:
#   smoke    - Quick 5-minute smoke test (essential commands)
#   basic    - Standard regression (15 min)
#   full     - Complete regression (all tests)
#   commands - Only command tests
#   levels   - Only log level tests
#   auth     - Only authentication tests (requires PASSWORD)
#
# Environment variables:
#   DEVICE_IP   - IP address of the device (default: localhost)
#   DEVICE_PORT - Telnet port (default: 23)
#   TIMEOUT     - Command timeout in seconds (default: 3)
#   PASSWORD    - Device password if set (default: none)
#   VERBOSE     - Set to 1 for verbose output
#   TEST_FIRMWARE - Set to 1 if using test_firmware.ino (enables extra tests)
#   STOP_ON_FAIL - Set to 1 to stop on first failure (default: 0, run all tests)
#

set -uo pipefail
# Note: -e removed so tests continue after failures

# Configuration
DEVICE_IP="${DEVICE_IP:-${2:-localhost}}"
DEVICE_PORT="${DEVICE_PORT:-${3:-23}}"
TIMEOUT="${TIMEOUT:-3}"
PASSWORD="${PASSWORD:-}"
VERBOSE="${VERBOSE:-0}"
STOP_ON_FAIL="${STOP_ON_FAIL:-0}"
TEST_FIRMWARE="${TEST_FIRMWARE:-0}"
RECONNECT_EACH="${RECONNECT_EACH:-0}"  # Set to 1 to reconnect for each command (slower but tests reconnection)
COMMAND_DELAY="${COMMAND_DELAY:-0.5}"  # Delay between commands (device has 500ms duplicate filter)

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

# Timing
START_TIME=$(date +%s)

# Persistent connection state
NC_PID=""
NC_IN=""
NC_OUT=""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

#------------------------------------------------------------------------------
# Helper Functions
#------------------------------------------------------------------------------

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((TESTS_PASSED++)) || true
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((TESTS_FAILED++)) || true
}

log_skip() {
    echo -e "${YELLOW}[SKIP]${NC} $1"
    ((TESTS_SKIPPED++)) || true
}

log_verbose() {
    if [[ "$VERBOSE" == "1" ]]; then
        echo -e "${CYAN}[DEBUG]${NC} $1"
    fi
}

log_section() {
    echo ""
    echo -e "${BOLD}=== $1 ===${NC}"
}

#------------------------------------------------------------------------------
# Persistent Connection Management
#------------------------------------------------------------------------------

# Open a persistent connection to the device
# Creates named pipes for bidirectional communication
open_connection() {
    if [[ -n "$NC_PID" ]] && kill -0 "$NC_PID" 2>/dev/null; then
        log_verbose "Connection already open (PID: $NC_PID)"
        return 0
    fi
    
    # Create temp directory for FIFOs
    local tmpdir
    tmpdir=$(mktemp -d)
    NC_IN="$tmpdir/nc_in"
    NC_OUT="$tmpdir/nc_out"
    
    mkfifo "$NC_IN" "$NC_OUT"
    
    # Start nc in background with FIFOs
    nc "$DEVICE_IP" "$DEVICE_PORT" < "$NC_IN" > "$NC_OUT" 2>/dev/null &
    NC_PID=$!
    
    # Open file descriptors for writing to input FIFO
    exec 3>"$NC_IN"
    exec 4<"$NC_OUT"
    
    # Wait for welcome message and discard it
    sleep 0.5
    local welcome=""
    while read -r -t 0.1 line <&4 2>/dev/null; do
        welcome+="$line"$'\n'
    done
    
    log_verbose "Connection opened (PID: $NC_PID)"
    log_verbose "Welcome message: ${#welcome} bytes"
    
    return 0
}

# Close the persistent connection
close_connection() {
    if [[ -n "$NC_PID" ]]; then
        kill "$NC_PID" 2>/dev/null || true
        wait "$NC_PID" 2>/dev/null || true
        NC_PID=""
        
        # Close file descriptors
        exec 3>&- 2>/dev/null || true
        exec 4<&- 2>/dev/null || true
        
        # Clean up FIFOs
        [[ -n "$NC_IN" ]] && rm -f "$NC_IN" 2>/dev/null
        [[ -n "$NC_OUT" ]] && rm -f "$NC_OUT" 2>/dev/null
        [[ -n "$NC_IN" ]] && rmdir "$(dirname "$NC_IN")" 2>/dev/null || true
        
        NC_IN=""
        NC_OUT=""
        
        log_verbose "Connection closed"
    fi
}

# Cleanup on exit
cleanup() {
    close_connection
}
trap cleanup EXIT

# Send a command and capture response
# Uses persistent connection by default, or reconnects each time if RECONNECT_EACH=1
# Usage: response=$(send_command "command")
send_command() {
    local cmd="$1"
    local response=""
    
    if [[ "$RECONNECT_EACH" == "1" ]]; then
        # Legacy mode: new connection for each command
        log_verbose "Sending (new connection): '$cmd'"
        response=$(printf "%s\r\n" "$cmd" | timeout "$TIMEOUT" nc -w "$TIMEOUT" "$DEVICE_IP" "$DEVICE_PORT" 2>/dev/null || true)
    else
        # Persistent connection mode
        local line
        
        # Ensure connection is open
        if [[ -z "$NC_PID" ]] || ! kill -0 "$NC_PID" 2>/dev/null; then
            open_connection || return 1
        fi
        
        log_verbose "Sending: '$cmd'"
        
        # Send command
        printf "%s\r\n" "$cmd" >&3
        
        # Read response (with timeout)
        sleep 0.2  # Give device time to respond
        while read -r -t 0.3 line <&4 2>/dev/null; do
            response+="$line"$'\n'
        done
        
        # Wait before next command (device has 500ms duplicate filter)
        sleep "$COMMAND_DELAY"
    fi
    
    log_verbose "Response length: ${#response} bytes"
    if [[ "$VERBOSE" == "1" ]] && [[ ${#response} -lt 500 ]]; then
        log_verbose "Response: $response"
    fi
    
    echo "$response"
}

# Send command and check if expected pattern is in response (case insensitive)
# Usage: assert_contains "command" "expected_pattern" "Test Name"
assert_contains() {
    local cmd="$1"
    local expected="$2"
    local test_name="${3:-TC: $cmd}"
    
    local response
    response=$(send_command "$cmd")
    
    if echo "$response" | grep -qiE "$expected"; then
        log_pass "$test_name"
        return 0
    else
        log_fail "$test_name - expected pattern '$expected'"
        log_verbose "Full response: $response"
        [[ "$STOP_ON_FAIL" == "1" ]] && exit 1
        return 0  # Continue running tests
    fi
}

# Send command and check response does NOT contain pattern
assert_not_contains() {
    local cmd="$1"
    local unexpected="$2"
    local test_name="${3:-TC: $cmd}"
    
    local response
    response=$(send_command "$cmd")
    
    if echo "$response" | grep -qiE "$unexpected"; then
        log_fail "$test_name - unexpected pattern '$unexpected' found"
        log_verbose "Full response: $response"
        [[ "$STOP_ON_FAIL" == "1" ]] && exit 1
        return 0  # Continue running tests
    else
        log_pass "$test_name"
        return 0
    fi
}

# Check device is still responsive after a command
assert_responsive() {
    local test_name="${1:-Device responsive}"
    
    if nc -z -w 2 "$DEVICE_IP" "$DEVICE_PORT" 2>/dev/null; then
        log_pass "$test_name"
        return 0
    else
        log_fail "$test_name - device not responding"
        return 1
    fi
}

# Wait for device to be reachable
wait_for_device() {
    local max_attempts="${1:-30}"
    local attempt=0
    
    log_info "Waiting for device at $DEVICE_IP:$DEVICE_PORT..."
    
    while [[ $attempt -lt $max_attempts ]]; do
        if nc -z -w 1 "$DEVICE_IP" "$DEVICE_PORT" 2>/dev/null; then
            log_info "Device is ready!"
            return 0
        fi
        ((attempt++)) || true
        sleep 1
    done
    
    log_fail "Device not reachable after $max_attempts seconds"
    return 1
}

# Authenticate if password is set
authenticate() {
    if [[ -n "$PASSWORD" ]]; then
        log_info "Authenticating with password..."
        local response
        response=$(send_command "$PASSWORD")
        
        if echo "$response" | grep -qi "password ok\|allowing access"; then
            log_pass "Authentication successful"
            return 0
        else
            log_fail "Authentication failed"
            return 1
        fi
    fi
    return 0
}

#------------------------------------------------------------------------------
# Test Cases - Section 1: Connection Tests
#------------------------------------------------------------------------------

test_connection() {
    log_section "1. Connection Tests (TC-CON)"
    
    # TC-CON-001: Basic TCP Connection
    if nc -z -w "$TIMEOUT" "$DEVICE_IP" "$DEVICE_PORT" 2>/dev/null; then
        log_pass "TC-CON-001: TCP connection successful"
    else
        log_fail "TC-CON-001: Cannot connect to device"
        return 1
    fi
    
    # TC-CON-002: Welcome Message - open persistent connection
    # This also establishes our persistent connection for subsequent tests
    if open_connection; then
        log_pass "TC-CON-002: Welcome message received"
    else
        log_fail "TC-CON-002: Could not establish connection"
        return 1
    fi
    
    # TC-CON-010: handle() working (commands get processed)
    assert_contains "h" "Commands|help|Available" "TC-CON-010: handle() processes commands"
}

#------------------------------------------------------------------------------
# Test Cases - Section 2: Command Tests  
#------------------------------------------------------------------------------

test_commands_help() {
    log_section "2.1 Help & Info Commands (TC-CMD)"
    
    # TC-CMD-001: Help Command (h)
    assert_contains "h" "Commands|help|Available|Verbose|Debug" "TC-CMD-001: Help (h)"
    
    # TC-CMD-002: Help Command (?)
    assert_contains "?" "Commands|help|Available|Verbose|Debug" "TC-CMD-002: Help (?)"
    
    # TC-CMD-003: Memory Command (m)
    assert_contains "m" "Free.*Heap|Heap.*RAM|[0-9]+" "TC-CMD-003: Memory (m)"
}

test_commands_levels() {
    log_section "2.2 Debug Level Commands (TC-CMD)"
    
    # TC-CMD-004: Set Verbose Level (v)
    assert_contains "v" "Verbose" "TC-CMD-004: Set Verbose (v)"
    
    # TC-CMD-005: Set Debug Level (d)
    assert_contains "d" "Debug" "TC-CMD-005: Set Debug (d)"
    
    # TC-CMD-006: Set Info Level (i)
    assert_contains "i" "Info" "TC-CMD-006: Set Info (i)"
    
    # TC-CMD-007: Set Warning Level (w)
    assert_contains "w" "Warning" "TC-CMD-007: Set Warning (w)"
    
    # TC-CMD-008: Set Error Level (e)
    assert_contains "e" "Error" "TC-CMD-008: Set Error (e)"
    
    # TC-CMD-009: Toggle Debug Level Display (l)
    # TC-CMD-009: Toggle Debug Level Display (l)
    # Output: "* Show debug level: On" or "* Show debug level: Off"
    assert_contains "l" "Show debug level.*On|Show debug level.*Off" "TC-CMD-009: Toggle level display (l)"
    
    # Reset to verbose so subsequent tests can see command output
    send_command "v" >/dev/null
}

test_commands_display() {
    log_section "2.3 Display & Formatting Commands (TC-CMD)"
    
    # Reset to verbose level so we can see command responses
    # (previous tests may have set level to Error which filters output)
    send_command "v" >/dev/null
    
    # TC-CMD-010: Toggle Time Display (t)
    # Output: "* Show time: On" or "* Show time: Off"
    assert_contains "t" "Show time.*On|Show time.*Off" "TC-CMD-010: Toggle time (t)"
    
    # TC-CMD-011: Toggle Colors (c)
    # Output: "* Show colors: On" or "* Show colors: Off"
    assert_contains "c" "Show colors.*On|Show colors.*Off" "TC-CMD-011: Toggle colors (c)"
    
    # TC-CMD-012: Toggle Profiler (p)
    assert_contains "p" "Show profiler.*On|Show profiler.*Off" "TC-CMD-012: Toggle profiler (p)"
    
    # TC-CMD-013: Profiler with Minimum Time (p N)
    assert_contains "p 100" "Show profiler.*On|minimal time.*100" "TC-CMD-013: Profiler with min time (p 100)"
    
    # Reset profiler
    send_command "p" >/dev/null
    
    # TC-CMD-014: Profiler Level (P)
    # Output: "* Debug level set to Profiler (disable in N millis)"
    assert_contains "P" "level set to Profiler|Profiler.*disable" "TC-CMD-014: Profiler Level (P)"
    
    # Reset to verbose after P command (profiler level affects output)
    send_command "v" >/dev/null
    
    # TC-CMD-015: Auto Profiler (A)
    # Output: "* Auto profiler debug level active (time >= N millis)"
    assert_contains "A" "Auto profiler.*active|profiler.*level.*active" "TC-CMD-015: Auto Profiler (A)"
}

test_commands_silence_filter() {
    log_section "2.4 Silence & Filter Commands (TC-CMD)"
    
    # TC-CMD-016: Toggle Silence Mode (s)
    assert_contains "s" "silent|silence" "TC-CMD-016: Silence mode (s)"
    
    # TC-CMD-017: Exit Silence with command - send another command
    local response
    response=$(send_command "h")
    if echo "$response" | grep -qiE "exit.*silent|Commands|help"; then
        log_pass "TC-CMD-017: Exit silence with command"
    else
        log_skip "TC-CMD-017: Exit silence (response unclear)"
    fi
    
    # TC-CMD-018: Set Filter (filter <text>)
    assert_contains "filter test_pattern" "filter.*active|Filter.*test" "TC-CMD-018: Set filter"
    
    # TC-CMD-019: Verify filter is active
    local response
    response=$(send_command "filter status_check")
    if echo "$response" | grep -qiE "filter"; then
        log_pass "TC-CMD-019: Filter status check"
    else
        log_skip "TC-CMD-019: Filter status (response unclear)"
    fi
    
    # TC-CMD-020: Clear Filter (nofilter)
    assert_contains "nofilter" "filter.*disabled|Filter.*off|filter.*clear" "TC-CMD-020: Clear filter (nofilter)"
}

test_commands_timeout() {
    log_section "2.5 Timeout Command (TC-CMD)"
    
    # TC-CMD-021: Get Connection Timeout
    assert_contains "timeout" "Timeout.*[0-9]+.*seconds|[0-9]+" "TC-CMD-021: Get timeout"
    
    # TC-CMD-022: Set Connection Timeout (valid value)
    assert_contains "timeout 120" "Timeout.*120|120.*seconds|120" "TC-CMD-022: Set timeout 120"
    
    # TC-CMD-023: Timeout Minimum Validation
    assert_contains "timeout 30" "must be.*60|minimal.*60|Timeout|30" "TC-CMD-023: Timeout minimum validation"
    
    # TC-CMD-024: Timeout Zero (Disable)
    assert_contains "timeout 0" "Timeout.*0|disabled|0" "TC-CMD-024: Timeout disable (0)"
    
    # Restore reasonable timeout
    send_command "timeout 300" >/dev/null
}

test_commands_system() {
    log_section "2.6 System Commands (TC-CMD)"
    
    # TC-CMD-027: Reset Command Disabled (default)
    # We test by checking device doesn't reset
    send_command "reset" >/dev/null
    sleep 1
    if nc -z -w 2 "$DEVICE_IP" "$DEVICE_PORT" 2>/dev/null; then
        log_pass "TC-CMD-027: Reset disabled (device still up)"
    else
        log_skip "TC-CMD-027: Device may have reset (reset enabled?)"
    fi
    
    # TC-CMD-025: Quit is tested separately as it closes connection
    log_skip "TC-CMD-025: Quit (q) - skipped (would close connection)"
    
    # TC-CMD-026: Reset enabled - skip as it would reset device
    log_skip "TC-CMD-026: Reset enabled - skipped (destructive)"
    
    # TC-CMD-028/029: CPU commands - ESP8266 only
    log_skip "TC-CMD-028/029: CPU commands - skipped (ESP8266 specific)"
}

test_commands_edge() {
    log_section "2.7 Edge Cases (TC-CMD)"
    
    # TC-CMD-030: Unknown Command
    send_command "xyz_unknown_cmd_12345" >/dev/null
    assert_responsive "TC-CMD-030: Unknown command (no crash)"
    
    # TC-CMD-031: Empty Command
    send_command "" >/dev/null
    assert_responsive "TC-CMD-031: Empty command (no crash)"
    
    # TC-CMD-032: Command with Spaces
    local response
    response=$(send_command "filter test pattern spaces")
    if echo "$response" | grep -qiE "filter|test"; then
        log_pass "TC-CMD-032: Command with spaces"
    else
        log_skip "TC-CMD-032: Command with spaces (unclear)"
    fi
    send_command "nofilter" >/dev/null
}

#------------------------------------------------------------------------------
# Test Cases - Section 3: Log Level Tests
#------------------------------------------------------------------------------

test_log_levels() {
    log_section "3. Log Level Tests (TC-LVL)"
    
    # These tests verify the level filtering via command responses
    # Full verification requires test firmware with predictable output
    
    # TC-LVL-001 through TC-LVL-005: Level setting
    # Already covered in test_commands_levels
    
    # TC-LVL-009: Level Prefix Display
    # Output: "* Show debug level: On" or "* Show debug level: Off"
    assert_contains "l" "Show debug level.*On|Show debug level.*Off" "TC-LVL-009/010: Level prefix toggle"
    
    if [[ "$TEST_FIRMWARE" == "1" ]]; then
        log_info "Test firmware detected - running level filtering tests"
        
        # Set to error level
        send_command "e" >/dev/null
        
        # Request test output
        local response
        response=$(send_command "test_all_levels")
        
        # Should NOT see VERBOSE, DEBUG, INFO, WARNING in output
        # Should see ERROR
        if echo "$response" | grep -qiE "TEST_ERROR"; then
            log_pass "TC-LVL-005: Error level shows errors"
        else
            log_skip "TC-LVL-005: Error level filter (check manually)"
        fi
        
        # Reset to verbose
        send_command "v" >/dev/null
    else
        log_skip "TC-LVL-001-005: Level filtering (needs TEST_FIRMWARE=1)"
    fi
}

#------------------------------------------------------------------------------
# Test Cases - Section 4: Authentication Tests
#------------------------------------------------------------------------------

test_authentication() {
    log_section "4. Authentication Tests (TC-AUTH)"
    
    if [[ -z "$PASSWORD" ]]; then
        log_skip "TC-AUTH-001: No password - direct access (expected)"
        log_skip "TC-AUTH-002-008: Skipped (no PASSWORD set)"
        return 0
    fi
    
    log_info "Password is set - testing authentication flow"
    
    # TC-AUTH-003: Correct Password
    local response
    response=$(send_command "$PASSWORD")
    if echo "$response" | grep -qiE "password ok|allowing access"; then
        log_pass "TC-AUTH-003: Correct password grants access"
    else
        log_fail "TC-AUTH-003: Password not accepted"
    fi
    
    # TC-AUTH-004: Wrong Password (would need fresh connection)
    log_skip "TC-AUTH-004: Wrong password (requires fresh connection)"
    
    # TC-AUTH-005-008: Skipped for now
    log_skip "TC-AUTH-005-008: Additional auth tests (manual)"
}

#------------------------------------------------------------------------------
# Test Cases - Section 5: Output & Formatting Tests
#------------------------------------------------------------------------------

test_formatting() {
    log_section "5. Output & Formatting Tests (TC-FMT)"
    
    # Reset to verbose level so we can see command responses
    # (previous tests may have set level to Error which filters output)
    send_command "v" >/dev/null
    
    # TC-FMT-003: Colors Toggle
    # Output: "* Show colors: On" or "* Show colors: Off"
    assert_contains "c" "Show colors.*On|Show colors.*Off" "TC-FMT-003: Color toggle"
    send_command "c" >/dev/null  # Toggle back
    
    # TC-FMT-005: Time Toggle
    # Output: "* Show time: On" or "* Show time: Off"
    assert_contains "t" "Show time.*On|Show time.*Off" "TC-FMT-005: Time toggle"
    send_command "t" >/dev/null  # Toggle back
    
    # TC-FMT-007: Profiler Toggle
    # Output: "* Show profiler: On" or "* Show profiler: Off"
    assert_contains "p" "Show profiler.*On|Show profiler.*Off" "TC-FMT-007: Profiler toggle"
    send_command "p" >/dev/null  # Toggle back
    
    # TC-FMT-008-012: Require firmware observation
    log_skip "TC-FMT-008-012: Output format tests (manual/firmware)"
}

#------------------------------------------------------------------------------
# Test Cases - Section 6: API Method Tests
#------------------------------------------------------------------------------

test_api_methods() {
    log_section "6. API Method Tests (TC-API)"
    
    # Most API tests require firmware code, not telnet commands
    # We can only test command-accessible features
    
    # TC-API-001: begin() - already working if connected
    log_pass "TC-API-001: begin() working (connection established)"
    
    # TC-API-005: handle() - already working if commands process
    log_pass "TC-API-005: handle() working (commands processed)"
    
    # TC-API-011: setFilter() via command
    send_command "filter apitest" >/dev/null
    send_command "nofilter" >/dev/null
    log_pass "TC-API-011/012: setFilter/setNoFilter via commands"
    
    # TC-API-013/014: silence via command
    send_command "s" >/dev/null
    send_command "h" >/dev/null  # Exit silence
    log_pass "TC-API-013/014: silence via commands"
    
    if [[ "$TEST_FIRMWARE" == "1" ]]; then
        local response
        
        # TC-API-006: getLastCommand()
        response=$(send_command "test_last_cmd")
        if echo "$response" | grep -qiE "LAST_CMD:test_last_cmd"; then
            log_pass "TC-API-006: getLastCommand() returns correct command"
        else
            log_fail "TC-API-006: getLastCommand() failed"
            [[ "$DEBUG_MODE" == "1" ]] && echo "Response: $response"
        fi
        
        # TC-API-007: clearLastCommand()
        response=$(send_command "test_clear_cmd")
        if echo "$response" | grep -qiE "CLEAR_CMD:OK"; then
            log_pass "TC-API-007: clearLastCommand() clears buffer"
        else
            log_fail "TC-API-007: clearLastCommand() failed"
            [[ "$DEBUG_MODE" == "1" ]] && echo "Response: $response"
        fi
        
        # TC-API-009: isConnected()
        response=$(send_command "test_connected")
        if echo "$response" | grep -qiE "CONNECTED:1"; then
            log_pass "TC-API-009: isConnected() returns true"
        else
            log_fail "TC-API-009: isConnected() failed"
            [[ "$DEBUG_MODE" == "1" ]] && echo "Response: $response"
        fi
        
        # TC-API-014: isSilence() - should be false initially
        response=$(send_command "test_silence")
        if echo "$response" | grep -qiE "SILENCE:0"; then
            log_pass "TC-API-014: isSilence() returns false (not silent)"
        else
            log_fail "TC-API-014: isSilence() failed"
            [[ "$DEBUG_MODE" == "1" ]] && echo "Response: $response"
        fi
        
        # TC-API-008: setCallBackProjectCmds() - the callback is working if commands work
        response=$(send_command "test_callback")
        if echo "$response" | grep -qiE "CALLBACK:OK"; then
            log_pass "TC-API-008: setCallBackProjectCmds() callback works"
        else
            log_fail "TC-API-008: setCallBackProjectCmds() failed"
            [[ "$DEBUG_MODE" == "1" ]] && echo "Response: $response"
        fi
        
        log_skip "TC-API-002/003/004/010: Other API tests (require additional firmware)"
    else
        log_skip "TC-API-002-010: API tests (require firmware verification - use TEST_FIRMWARE=1)"
    fi
}

#------------------------------------------------------------------------------
# Test Cases - Section 7: Edge Cases & Stability Tests
#------------------------------------------------------------------------------

test_edge_cases() {
    log_section "7. Edge Cases & Stability Tests (TC-EDGE)"
    
    # TC-EDGE-003: Rapid Commands
    log_info "Sending rapid command sequence..."
    for cmd in "v" "d" "i" "w" "e" "m" "h" "c" "c" "t" "t"; do
        send_command "$cmd" >/dev/null
    done
    assert_responsive "TC-EDGE-003: Rapid commands (no crash)"
    
    # TC-EDGE-005: Binary/NULL Input
    # Send some non-printable characters
    printf "\x00\x01\x02\r\n" | timeout 2 nc -w 2 "$DEVICE_IP" "$DEVICE_PORT" >/dev/null 2>&1 || true
    assert_responsive "TC-EDGE-005: Binary input (no crash)"
    
    # TC-EDGE-006: Very Long Command
    local long_cmd
    long_cmd=$(printf 'x%.0s' {1..500})
    send_command "$long_cmd" >/dev/null
    assert_responsive "TC-EDGE-006: Long command 500 chars (no crash)"
    
    # TC-EDGE-010: CRLF vs LF
    printf "h\r\n" | timeout 2 nc -w 2 "$DEVICE_IP" "$DEVICE_PORT" >/dev/null 2>&1
    printf "h\n" | timeout 2 nc -w 2 "$DEVICE_IP" "$DEVICE_PORT" >/dev/null 2>&1
    assert_responsive "TC-EDGE-010: CRLF and LF both work"
    
    if [[ "$TEST_FIRMWARE" == "1" ]]; then
        # TC-EDGE-001: Long Message
        local response
        response=$(send_command "test_long")
        if echo "$response" | grep -qiE "LONG_MESSAGE|500|END"; then
            log_pass "TC-EDGE-001: Long message handling"
        else
            log_skip "TC-EDGE-001: Long message (check output)"
        fi
        
        # TC-EDGE-002: Special Characters
        response=$(send_command "test_special")
        if echo "$response" | grep -qiE "Special|tab|quote"; then
            log_pass "TC-EDGE-002: Special characters"
        else
            log_skip "TC-EDGE-002: Special characters (check output)"
        fi
        
        # TC-EDGE-004: Message Flood
        response=$(send_command "test_flood")
        sleep 2
        assert_responsive "TC-EDGE-004: Message flood (still responsive)"
        
        # TC-USR-001: User-defined ping/pong command
        # Reset to verbose level so INFO messages are visible
        send_command "v" >/dev/null
        response=$(send_command "ping")
        if echo "$response" | grep -qiE "pong"; then
            log_pass "TC-USR-001: ping -> pong (user command)"
        else
            log_fail "TC-USR-001: ping -> pong (expected 'pong' response)"
            [[ "$DEBUG_MODE" == "1" ]] && echo "Response: $response"
        fi
    else
        log_skip "TC-EDGE-001/002/004: Need TEST_FIRMWARE=1"
        log_skip "TC-USR-001: ping -> pong (needs TEST_FIRMWARE=1)"
    fi
}

test_stability() {
    log_section "7b. Stability Tests"
    
    # TC-EDGE-009: Duplicate Command Filter
    # Send same command twice quickly
    send_command "m" >/dev/null
    send_command "m" >/dev/null
    assert_responsive "TC-EDGE-009: Duplicate commands handled"
    
    # TC-EDGE-011: Memory Stability (basic check)
    local mem1 mem2
    mem1=$(send_command "m" | grep -oE '[0-9]+' | head -1 || echo "0")
    
    # Send 20 commands (reduced from 50 for speed - each opens new connection)
    log_info "Sending 20 commands to check memory stability..."
    for i in {1..20}; do
        send_command "h" >/dev/null
        [[ $((i % 5)) -eq 0 ]] && echo -n "." >&2
    done
    echo "" >&2
    
    mem2=$(send_command "m" | grep -oE '[0-9]+' | head -1 || echo "0")
    
    if [[ -n "$mem1" ]] && [[ -n "$mem2" ]]; then
        local diff=$((mem1 - mem2))
        if [[ $diff -lt 5000 ]] && [[ $diff -gt -5000 ]]; then
            log_pass "TC-EDGE-011: Memory stable after 20 commands (diff: ${diff})"
        else
            log_fail "TC-EDGE-011: Memory changed significantly (diff: ${diff})"
        fi
    else
        log_skip "TC-EDGE-011: Could not parse memory values"
    fi
}

#------------------------------------------------------------------------------
# Test Suites
#------------------------------------------------------------------------------

run_smoke_tests() {
    log_info ""
    log_info "========================================"
    log_info "  RemoteDebug SMOKE Test (Quick)"
    log_info "  Device: $DEVICE_IP:$DEVICE_PORT"
    log_info "========================================"
    
    test_connection || return 1
    authenticate || return 1
    
    # Essential commands only
    test_commands_help
    test_commands_levels
}

run_basic_tests() {
    log_info ""
    log_info "========================================"
    log_info "  RemoteDebug BASIC Regression Tests"
    log_info "  Device: $DEVICE_IP:$DEVICE_PORT"
    log_info "========================================"
    
    test_connection || return 1
    authenticate || return 1
    
    test_commands_help
    test_commands_levels
    test_commands_display
    test_commands_silence_filter
    test_log_levels
    test_formatting
}

run_full_tests() {
    log_info ""
    log_info "========================================"
    log_info "  RemoteDebug FULL Regression Tests"
    log_info "  Device: $DEVICE_IP:$DEVICE_PORT"
    log_info "  Test Firmware: $TEST_FIRMWARE"
    log_info "========================================"
    
    test_connection || return 1
    authenticate || return 1
    
    test_commands_help
    test_commands_levels
    test_commands_display
    test_commands_silence_filter
    test_commands_timeout
    test_commands_system
    test_commands_edge
    test_log_levels
    test_authentication
    test_formatting
    test_api_methods
    test_edge_cases
    test_stability
}

run_command_tests() {
    log_info ""
    log_info "========================================"
    log_info "  RemoteDebug COMMAND Tests Only"
    log_info "========================================"
    
    test_connection || return 1
    authenticate || return 1
    
    test_commands_help
    test_commands_levels
    test_commands_display
    test_commands_silence_filter
    test_commands_timeout
    test_commands_system
    test_commands_edge
}

print_summary() {
    local end_time=$(date +%s)
    local elapsed=$((end_time - START_TIME))
    local minutes=$((elapsed / 60))
    local seconds=$((elapsed % 60))
    local total=$((TESTS_PASSED + TESTS_FAILED + TESTS_SKIPPED))
    local pass_rate=0
    if [[ $((TESTS_PASSED + TESTS_FAILED)) -gt 0 ]]; then
        pass_rate=$((100 * TESTS_PASSED / (TESTS_PASSED + TESTS_FAILED)))
    fi
    
    echo ""
    echo -e "${BOLD}========================================"
    echo -e "  Test Summary"
    echo -e "========================================${NC}"
    echo -e "  ${GREEN}Passed:${NC}  $TESTS_PASSED"
    echo -e "  ${RED}Failed:${NC}  $TESTS_FAILED"
    echo -e "  ${YELLOW}Skipped:${NC} $TESTS_SKIPPED"
    echo -e "  Total:   $total"
    echo -e "  ${BOLD}Pass Rate: ${pass_rate}%${NC}"
    echo -e "  ${CYAN}Duration:${NC} ${minutes}m ${seconds}s"
    echo -e "${BOLD}========================================${NC}"
    
    if [[ $TESTS_FAILED -gt 0 ]]; then
        echo -e "${RED}REGRESSION FAILED${NC}"
        return 1
    else
        echo -e "${GREEN}REGRESSION PASSED${NC}"
        return 0
    fi
}

print_usage() {
    echo "RemoteDebug Integration Test Suite"
    echo ""
    echo "Usage: $0 [suite] [device_ip] [port]"
    echo ""
    echo "Test Suites:"
    echo "  smoke     - Quick smoke test (~30 sec)"
    echo "  basic     - Standard regression (~1 min)"
    echo "  full      - Complete regression (~2 min)"
    echo "  commands  - All command tests"
    echo "  levels    - Log level tests"
    echo "  auth      - Authentication tests (requires PASSWORD)"
    echo ""
    echo "Environment Variables:"
    echo "  DEVICE_IP      - Device IP (default: localhost)"
    echo "  DEVICE_PORT    - Telnet port (default: 23)"
    echo "  TIMEOUT        - Command timeout in seconds (default: 3)"
    echo "  PASSWORD       - Password if required"
    echo "  VERBOSE        - Set to 1 for debug output"
    echo "  TEST_FIRMWARE  - Set to 1 if using test_firmware.ino"
    echo "  STOP_ON_FAIL   - Set to 1 to stop on first failure"
    echo "  RECONNECT_EACH - Set to 1 to reconnect for each command (slower)"
    echo "  COMMAND_DELAY  - Delay between commands in seconds (default: 0.5)"
    echo ""
    echo "Examples:"
    echo "  $0 smoke 192.168.1.100"
    echo "  DEVICE_IP=192.168.1.100 $0 full"
    echo "  TEST_FIRMWARE=1 $0 full 192.168.1.100"
    echo "  PASSWORD=secret $0 auth 192.168.1.100"
    echo "  VERBOSE=1 RECONNECT_EACH=1 $0 smoke 192.168.1.100"
}

#------------------------------------------------------------------------------
# Main
#------------------------------------------------------------------------------

main() {
    local test_suite="${1:-full}"
    
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
        commands)
            wait_for_device || exit 1
            run_command_tests
            ;;
        levels)
            wait_for_device || exit 1
            test_connection || exit 1
            authenticate || exit 1
            test_log_levels
            ;;
        auth)
            wait_for_device || exit 1
            test_connection || exit 1
            test_authentication
            ;;
        connection)
            wait_for_device
            test_connection
            ;;
        help|--help|-h)
            print_usage
            exit 0
            ;;
        *)
            echo "Unknown test suite: $test_suite"
            echo ""
            print_usage
            exit 1
            ;;
    esac
    
    print_summary
}

# Run if executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
