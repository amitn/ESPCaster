# Default WiFi Configuration

This document explains how to configure default WiFi credentials for the ESPCast project.

## Overview

The ESPCast project now supports default WiFi credentials that are tried first before falling back to saved credentials or manual configuration. This is useful for:

- Development environments where you want to automatically connect to a known network
- Production deployments where devices should connect to a specific network by default
- Reducing setup time for new devices

## Configuration

### Method 1: Using menuconfig (Recommended)

1. Run the ESP-IDF configuration menu:
   ```bash
   idf.py menuconfig
   ```

2. Navigate to: `Example Configuration` → `Default WiFi Configuration`

3. Enable "Enable default WiFi credentials"

4. Set your WiFi SSID and password:
   - **Default WiFi SSID**: Enter your network name
   - **Default WiFi Password**: Enter your network password

5. Save and exit the configuration menu

### Method 2: Direct sdkconfig.defaults editing

Edit the `sdkconfig.defaults` file and add/uncomment these lines:

```
CONFIG_DEFAULT_WIFI_ENABLED=y
CONFIG_DEFAULT_WIFI_SSID="YourWiFiNetwork"
CONFIG_DEFAULT_WIFI_PASSWORD="YourWiFiPassword"
```

Replace `YourWiFiNetwork` and `YourWiFiPassword` with your actual WiFi credentials.

## How It Works

### Connection Priority

When auto-connect is enabled, the system tries to connect in this order:

1. **Default credentials** (from project configuration)
2. **Saved credentials** (from NVS storage)
3. **Manual configuration** (via GUI)

### Auto-Connect Behavior

- On system startup, if auto-connect is enabled, the system will first try the default credentials
- If default credentials fail or are not configured, it falls back to saved credentials
- If both fail, the user can manually scan and connect via the GUI

### Reconnection Logic

When the WiFi connection is lost, the auto-reconnect feature will:

1. Try default credentials first
2. Fall back to saved credentials if default fails
3. Retry up to 5 times before giving up

## Security Considerations

⚠️ **Important Security Notes:**

- Default credentials are stored in the firmware binary
- Anyone with access to the firmware can extract these credentials
- Only use this feature in trusted environments
- Consider using saved credentials (NVS) for production deployments with sensitive networks

## Testing

You can test the default WiFi functionality using the provided test functions:

```c
// Test default WiFi connection specifically
esp_cast_test_default_wifi();

// Test full auto-connect (default + saved credentials)
esp_cast_test_wifi_auto_connect();
```

## API Reference

### New Functions

#### `wifi_manager_try_default_credentials()`

Attempts to connect using default WiFi credentials from project configuration.

**Returns:**
- `ESP_OK`: Connection initiated successfully
- `ESP_ERR_NOT_FOUND`: No default credentials configured
- Other ESP error codes on failure

#### `esp_cast_test_default_wifi()`

Test function to verify default WiFi credentials functionality.

### Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_DEFAULT_WIFI_ENABLED` | bool | false | Enable default WiFi credentials |
| `CONFIG_DEFAULT_WIFI_SSID` | string | "" | Default WiFi network SSID |
| `CONFIG_DEFAULT_WIFI_PASSWORD` | string | "" | Default WiFi network password |

## Example Usage

### Development Setup

For development, you might want to automatically connect to your home/office WiFi:

```
CONFIG_DEFAULT_WIFI_ENABLED=y
CONFIG_DEFAULT_WIFI_SSID="DevNetwork"
CONFIG_DEFAULT_WIFI_PASSWORD="dev123456"
```

### Production Deployment

For production, you might want to connect to a specific customer network:

```
CONFIG_DEFAULT_WIFI_ENABLED=y
CONFIG_DEFAULT_WIFI_SSID="CustomerWiFi"
CONFIG_DEFAULT_WIFI_PASSWORD="customer_password"
```

## Troubleshooting

### Default WiFi Not Connecting

1. Check that `CONFIG_DEFAULT_WIFI_ENABLED=y` is set
2. Verify SSID and password are correct
3. Ensure the network is available and in range
4. Check logs for connection error messages

### Logs to Monitor

Look for these log messages:

```
I (xxx) wifi_manager: Trying to connect to default WiFi network: YourNetwork
I (xxx) wifi_manager: Default WiFi credentials not enabled in configuration
I (xxx) wifi_manager: No default WiFi credentials configured
```

### Common Issues

- **Empty SSID**: Make sure `CONFIG_DEFAULT_WIFI_SSID` is not empty
- **Wrong credentials**: Double-check SSID and password spelling
- **Network not available**: Ensure the WiFi network is broadcasting and in range
- **Security mismatch**: Verify the network security type is supported

## Integration with Existing Code

The default WiFi feature integrates seamlessly with existing code:

- No changes needed to existing WiFi GUI functionality
- Saved credentials still work as before
- Manual connection via GUI remains unchanged
- All existing callbacks and status updates continue to work

The feature is backward compatible and can be disabled by setting `CONFIG_DEFAULT_WIFI_ENABLED=n`.
