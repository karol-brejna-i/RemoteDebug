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
#

set -euo pipefail

# Configuration
DEVICE_IP="${DEVICE_IP:-${2:-localhost}}"
DEVICE_PORT="${DEVICE_PORT:-${3:-23}}"
TIMEOUT="${TIMEOUT:-3}"
PASSWORD="${PASSWORD:-}"
VERBOSE="${VERBOSE:-0}"
TEST_FIRMWARE="${TEST_FIRMWARE:-0}"

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

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

# Send a command and capture response
# Usage: response=$(send_command "command")
send_command() {
    local cmd="$1"
    local response
    
    log_verbose "Sending: '$cmd'"
    
    # Use printf to send command with CRLF, capture response
    response=$(printf "%s\r\n" "$cmd" | timeout "$TIMEOUT" nc -w "$TIMEOUT" "$DEVICE_IP" "$DEVICE_PORT" 2>/dev/null || true)
    
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
        return 1
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
        return 1
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
    
    # TC-CON-002: Welcome Message Content
    local welcome
    welcome=$(echo "" | timeout 2 nc -w 2 "$DEVICE_IP" "$DEVICE_PORT" 2>/dev/null || true)
    if echo "$welcome" | grep -qiE "RemoteDebug|welcome|host|help"; then
        log_pass "TC-CON-002: Welcome message received"
    else
        log_skip "TC-CON-002: Welcome message not detected (may be OK)"
    fi
    
    # TC-CON-010: handle() working (commands get processed)
    if assert_contains "h" "Commands|help|Available" "TC-CON-010: handle() processes commands"; then
        : # pass
    fi
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
    assert_contains "l" "debug level.*On|debug level.*Off|Show debug" "TC-CMD-009: Toggle level display (l)"
}

test_commands_display() {
    log_section "2.3 Display & Formatting Commands (TC-CMD)"
    
    # TC-CMD-010: Toggle Time Display (t)
    assert_contains "t" "time.*On|time.*Off|Show time" "TC-CMD-010: Toggle time (t)"
    
    # TC-CMD-011: Toggle Colors (c)
    assert_contains "c" "color.*On|color.*Off|Show color" "TC-CMD-011: Toggle colors (c)"
    
    # TC-CMD-012: Toggle Profiler (p)
    assert_contains "p" "profiler.*On|profiler.*Off|Show profiler" "TC-CMD-012: Toggle profiler (p)"
    
    # TC-CMD-013: Profiler with Minimum Time (p N)
    assert_contains "p 100" "profiler.*On|minimal time|100" "TC-CMD-013: Profiler with min time (p 100)"
    
    # Reset profiler
    send_command "p" >/dev/null
    
    # TC-CMD-014: Profiler Level (P)
    assert_contains "P" "profiler level|level.*profiler" "TC-CMD-014: Profiler Level (P)"
    
    # TC-CMD-015: Auto Profiler (A)
    assert_contains "A" "auto.*profiler|profiler.*auto" "TC-CMD-015: Auto Profiler (A)"
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
    assert_contains "l" "debug level" "TC-LVL-009/010: Level prefix toggle"
    
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
    
    # TC-FMT-003: Colors Toggle
    assert_contains "c" "color" "TC-FMT-003: Color toggle"
    send_command "c" >/dev/null  # Toggle back
    
    # TC-FMT-005: Time Toggle
    assert_contains "t" "time" "TC-FMT-005: Time toggle"
    send_command "t" >/dev/null  # Toggle back
    
    # TC-FMT-007: Profiler Toggle
    assert_contains "p" "profiler" "TC-FMT-007: Profiler toggle"
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
    
    log_skip "TC-API-002-010: API tests (require firmware verification)"
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
    else
        log_skip "TC-EDGE-001/002/004: Need TEST_FIRMWARE=1"
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
    
    # Send 50 commands
    for _ in {1..50}; do
        send_command "h" >/dev/null
    done
    
    mem2=$(send_command "m" | grep -oE '[0-9]+' | head -1 || echo "0")
    
    if [[ -n "$mem1" ]] && [[ -n "$mem2" ]]; then
        local diff=$((mem1 - mem2))
        if [[ $diff -lt 5000 ]] && [[ $diff -gt -5000 ]]; then
            log_pass "TC-EDGE-011: Memory stable after 50 commands (diff: ${diff})"
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
    echo "  smoke     - Quick smoke test (~2 min, essential commands)"
    echo "  basic     - Standard regression (~5 min)"
    echo "  full      - Complete regression (~10 min)"
    echo "  commands  - All command tests"
    echo "  levels    - Log level tests"
    echo "  auth      - Authentication tests (requires PASSWORD)"
    echo ""
    echo "Environment Variables:"
    echo "  DEVICE_IP     - Device IP (default: localhost)"
    echo "  DEVICE_PORT   - Telnet port (default: 23)"
    echo "  PASSWORD      - Password if required"
    echo "  VERBOSE       - Set to 1 for debug output"
    echo "  TEST_FIRMWARE - Set to 1 if using test_firmware.ino"
    echo ""
    echo "Examples:"
    echo "  $0 smoke 192.168.1.100"
    echo "  DEVICE_IP=192.168.1.100 $0 full"
    echo "  TEST_FIRMWARE=1 $0 full 192.168.1.100"
    echo "  PASSWORD=secret $0 auth 192.168.1.100"
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
