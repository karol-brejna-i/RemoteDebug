Import("env")
import os

# Get project directory and set integration test source
project_dir = env.get("PROJECT_DIR")
integration_src = os.path.join(project_dir, "test", "integration")

# Override source directory
env["PROJECT_SRC_DIR"] = integration_src

# Load .env file if it exists
env_file = os.path.join(project_dir, ".env")
if os.path.exists(env_file):
    print(f"Loading environment from {env_file}")
    with open(env_file, "r") as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                key, value = line.split("=", 1)
                key = key.strip()
                value = value.strip().strip('"').strip("'")
                os.environ[key] = value
                print(f"  Loaded: {key}={'*' * len(value)}")

# Get WiFi credentials from environment
wifi_ssid = os.environ.get("WIFI_SSID", "")
wifi_password = os.environ.get("WIFI_PASSWORD", "")

# Add build flags for WiFi credentials
if wifi_ssid:
    env.Append(CPPDEFINES=[
        ("WIFI_SSID", env.StringifyMacro(wifi_ssid)),
        ("WIFI_PASSWORD", env.StringifyMacro(wifi_password))
    ])
    print(f"  WiFi SSID: {wifi_ssid}")
else:
    print("  WARNING: WIFI_SSID not set!")
