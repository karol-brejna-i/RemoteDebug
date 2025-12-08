# Integration Tests Migration Plan

## Current State

The integration test suite consists of two bash scripts:
- `run_tests.sh` (~1000 lines) - Telnet-based tests
- `run_tests_ws.sh` (~530 lines) - WebSocket-based tests

### Issues with Current Approach

1. **Maintainability**: 500+ lines of bash is difficult to maintain and extend
2. **Duplication**: Connection management, assertions, and reporting logic duplicated between scripts
3. **Limited assertions**: Only grep pattern matching, no structured assertions
4. **Fragile connections**: Manual FIFO/pipe management for persistent connections
5. **No test isolation**: Tests share connection state, failures can cascade
6. **Debugging difficulty**: Hard to debug failures without structured output
7. **No parallel execution**: Tests run sequentially
8. **Limited reporting**: No JUnit XML or other CI-friendly formats

---

## Recommendation: Migrate to pytest

### Why pytest?

1. **Already using Python**: `set_src_dir.py` and WebSocket testing already use Python
2. **PlatformIO integration**: Native pytest support for CI pipelines
3. **Rich fixture model**: Clean setup/teardown for device connections
4. **Parameterized tests**: Eliminate repetitive test code (e.g., debug level tests)
5. **JUnit XML output**: Direct integration with GitHub Actions
6. **Good IDE support**: Debugging, test discovery, inline results
7. **Moderate complexity**: Not overkill for project size (unlike Robot Framework)

---

## Proposed Structure

```
test/
├── integration/
│   ├── conftest.py              # pytest fixtures
│   ├── pytest.ini               # pytest configuration
│   ├── requirements.txt         # Python dependencies
│   │
│   ├── clients/
│   │   ├── __init__.py
│   │   ├── telnet_client.py     # Reusable TelnetClient class
│   │   └── ws_client.py         # Reusable WebSocketClient class
│   │
│   ├── test_connection.py       # TC-CON-* tests
│   ├── test_commands_help.py    # TC-CMD-001-003, TC-CMD-020-023
│   ├── test_commands_levels.py  # TC-CMD-004-009
│   ├── test_commands_display.py # TC-CMD-010-017
│   ├── test_log_levels.py       # TC-LVL-* tests
│   ├── test_api.py              # TC-API-* tests
│   ├── test_edge_cases.py       # TC-EDGE-* tests
│   ├── test_websocket.py        # TC-WS-* tests
│   │
│   └── legacy/                  # Keep old scripts during migration
│       ├── run_tests.sh
│       └── run_tests_ws.sh
```

---

## Example Test Code

### conftest.py - Fixtures

```python
import pytest
import os
from clients.telnet_client import TelnetClient
from clients.ws_client import WebSocketClient

def pytest_addoption(parser):
    parser.addoption("--device-ip", required=True, help="Device IP address")
    parser.addoption("--telnet-port", default=23, type=int)
    parser.addoption("--ws-port", default=8232, type=int)
    parser.addoption("--test-firmware", action="store_true")

@pytest.fixture(scope="session")
def device_ip(request):
    return request.config.getoption("--device-ip")

@pytest.fixture(scope="function")
def telnet(device_ip, request):
    """Fresh telnet connection for each test."""
    port = request.config.getoption("--telnet-port")
    client = TelnetClient(device_ip, port)
    client.connect()
    yield client
    client.close()

@pytest.fixture(scope="function")
def websocket(device_ip, request):
    """Fresh WebSocket connection for each test."""
    port = request.config.getoption("--ws-port")
    client = WebSocketClient(device_ip, port)
    client.connect()
    yield client
    client.close()

@pytest.fixture
def test_firmware(request):
    """Check if test firmware features are available."""
    return request.config.getoption("--test-firmware")
```

### test_commands_help.py - Help Command Tests

```python
import pytest

class TestHelpCommands:
    """TC-CMD-001 through TC-CMD-003: Help & Info Commands"""
    
    def test_help_h_shows_commands(self, telnet):
        """TC-CMD-001: Help command (h) displays available commands."""
        response = telnet.send("h")
        assert "Commands" in response
        assert "verbose" in response.lower()
        assert "debug" in response.lower()
    
    def test_help_question_mark(self, telnet):
        """TC-CMD-002: Help command (?) works same as 'h'."""
        response = telnet.send("?")
        assert "Commands" in response
    
    def test_memory_command(self, telnet):
        """TC-CMD-003: Memory command shows free heap."""
        response = telnet.send("m")
        assert "Free Heap" in response or "free" in response.lower()


class TestProjectCommandsInHelp:
    """TC-CMD-020 through TC-CMD-023: Project Commands in Help"""
    
    @pytest.mark.test_firmware
    def test_project_commands_section_exists(self, telnet):
        """TC-CMD-020: Help shows 'Project commands' section."""
        response = telnet.send("h")
        assert "Project commands" in response
    
    @pytest.mark.test_firmware
    def test_documented_commands_in_help(self, telnet):
        """TC-CMD-021: Documented commands appear in help."""
        response = telnet.send("h")
        assert "test_all_levels" in response
        assert "ping" in response
    
    @pytest.mark.test_firmware
    def test_hidden_command_not_in_help(self, telnet):
        """TC-CMD-022: Hidden command not shown in help."""
        response = telnet.send("h")
        assert "hidden_cmd" not in response
    
    @pytest.mark.test_firmware
    def test_hidden_command_still_executes(self, telnet):
        """TC-CMD-023: Hidden command works despite not being in help."""
        response = telnet.send("hidden_cmd")
        assert "HIDDEN_CMD_EXECUTED" in response
```

### test_commands_levels.py - Debug Level Tests

```python
import pytest

class TestDebugLevels:
    """TC-CMD-004 through TC-CMD-008: Debug Level Commands"""
    
    @pytest.mark.parametrize("command,expected_level", [
        ("v", "Verbose"),
        ("d", "Debug"),
        ("i", "Info"),
        ("w", "Warning"),
        ("e", "Error"),
    ])
    def test_set_debug_level(self, telnet, command, expected_level):
        """TC-CMD-004-008: Setting debug levels."""
        response = telnet.send(command)
        assert expected_level in response
```

### clients/telnet_client.py

```python
import socket
import time

class TelnetClient:
    def __init__(self, host: str, port: int = 23, timeout: float = 5.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.socket = None
    
    def connect(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.settimeout(self.timeout)
        self.socket.connect((self.host, self.port))
        # Read and discard welcome message
        time.sleep(0.3)
        self._read_available()
    
    def send(self, command: str, wait: float = 0.3) -> str:
        """Send command and return response."""
        self.socket.sendall(f"{command}\r\n".encode())
        time.sleep(wait)
        return self._read_available()
    
    def _read_available(self) -> str:
        """Read all available data from socket."""
        data = b""
        self.socket.setblocking(False)
        try:
            while True:
                chunk = self.socket.recv(4096)
                if not chunk:
                    break
                data += chunk
        except BlockingIOError:
            pass
        finally:
            self.socket.setblocking(True)
        return data.decode("utf-8", errors="replace")
    
    def close(self):
        if self.socket:
            self.socket.close()
            self.socket = None
```

---

## Running Tests

```bash
# Install dependencies
pip install pytest pytest-timeout websocket-client

# Run all tests
pytest test/integration/ --device-ip=192.168.1.100 --test-firmware -v

# Run specific test file
pytest test/integration/test_commands_help.py --device-ip=192.168.1.100 -v

# Generate JUnit XML for CI
pytest test/integration/ --device-ip=192.168.1.100 --junitxml=test-results.xml

# Run only tests marked for test firmware
pytest -m test_firmware --device-ip=192.168.1.100

# Parallel execution (with pytest-xdist)
pytest test/integration/ --device-ip=192.168.1.100 -n 4
```

---

## Migration Path

### Phase 1: Foundation (Week 1)
- [ ] Create `clients/telnet_client.py` and `clients/ws_client.py`
- [ ] Create `conftest.py` with basic fixtures
- [ ] Migrate TC-CMD-001-003 (help commands) as proof of concept
- [ ] Verify both old bash and new pytest work in parallel

### Phase 2: Core Tests (Week 2)
- [ ] Migrate TC-CMD-* (all command tests)
- [ ] Migrate TC-LVL-* (log level tests)
- [ ] Migrate TC-CON-* (connection tests)

### Phase 3: Advanced Tests (Week 3)
- [ ] Migrate TC-API-* (API method tests)
- [ ] Migrate TC-EDGE-* (edge case tests)
- [ ] Migrate TC-WS-* (WebSocket tests)

### Phase 4: Cleanup (Week 4)
- [ ] Update CI/CD to use pytest
- [ ] Update documentation
- [ ] Move old bash scripts to `legacy/` or remove
- [ ] Add pytest to `requirements.txt` or PlatformIO test dependencies

---

## CI Integration

### GitHub Actions Example

```yaml
- name: Run Integration Tests
  run: |
    pip install pytest websocket-client
    pytest test/integration/ \
      --device-ip=${{ secrets.TEST_DEVICE_IP }} \
      --test-firmware \
      --junitxml=test-results.xml \
      -v
      
- name: Upload Test Results
  uses: actions/upload-artifact@v3
  with:
    name: test-results
    path: test-results.xml
```

---

## Benefits After Migration

| Aspect | Before (bash) | After (pytest) |
|--------|---------------|----------------|
| Lines of code | ~1500 total | ~500 total (estimated) |
| Adding new test | Copy/paste pattern | Add method to class |
| Connection handling | Manual FIFO/pipes | Fixture auto-management |
| Assertions | `grep -qiE` | Native `assert` with diff |
| Debugging | `echo` statements | IDE breakpoints |
| CI integration | Parse stdout | JUnit XML native |
| Parallel tests | Not supported | `pytest-xdist` |
| Test isolation | Shared state | Fresh fixture per test |
