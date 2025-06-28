# Spotify Integration Guide for ESPCast

This guide explains how to set up and use the Spotify integration in the ESPCast project.

## Overview

The Spotify integration allows you to:
- Authenticate with Spotify using OAuth2 with PKCE
- Browse your Spotify playlists
- Search for tracks
- Control playback
- Cast Spotify content to Chromecast devices

## Prerequisites

1. **Spotify Developer Account**: You need a Spotify app with client credentials
2. **WiFi Connection**: The ESP32 must be connected to WiFi
3. **Chromecast Devices**: Optional, for casting functionality

## Setup Instructions

### 1. Create Spotify App

1. Go to [Spotify Developer Dashboard](https://developer.spotify.com/dashboard)
2. Create a new app
3. Note down your `Client ID` and `Client Secret`
4. Add redirect URI: `http://localhost:8888/callback`

### 2. Configure ESPCast

Add the following to your main application initialization:

```c
#include "esp_cast.h"

void app_main(void) {
    // ... existing initialization code ...
    
    // Initialize Spotify with your app credentials
    bool spotify_init_success = esp_cast_spotify_init(
        "your_spotify_client_id",
        "your_spotify_client_secret",  // Optional for PKCE flow
        "http://localhost:8888/callback"
    );
    
    if (spotify_init_success) {
        ESP_LOGI("APP", "Spotify integration initialized successfully");
    } else {
        ESP_LOGE("APP", "Failed to initialize Spotify integration");
    }
    
    // ... rest of initialization ...
}
```

### 3. Build Configuration

Make sure your `CMakeLists.txt` includes the Spotify component:

```cmake
REQUIRES
    "spotify_controller"
    # ... other requirements ...
```

## Usage Workflow

### 1. Authentication

1. Navigate to the "Spotify" tab in the GUI
2. Click "Authenticate" button
3. The system will display an authentication URL
4. Visit the URL in a web browser on any device
5. Log in to Spotify and authorize the app
6. The ESP32 will automatically receive the authorization and connect

### 2. Browse Playlists

Once authenticated:
1. Your playlists will automatically load
2. Click on any playlist to view its tracks
3. Use the back button to return to the playlist view

### 3. Play Music

For each track, you have two options:
1. **Play on Spotify**: Plays on your active Spotify device
2. **Cast to Chromecast**: Shows available Chromecast devices for casting

### 4. Chromecast Integration

When casting to Chromecast:
1. Select "Cast to Chromecast" for any track
2. Choose from available Chromecast devices
3. The track will be cast to the selected device

## API Reference

### Main Functions

```c
// Initialize Spotify integration
bool esp_cast_spotify_init(const char* client_id, const char* client_secret, const char* redirect_uri);

// Get Spotify controller handle
spotify_controller_handle_t esp_cast_get_spotify_controller(void);

// Cast to Chromecast
bool esp_cast_spotify_to_chromecast(const char* device_name, const char* track_uri);

// Get available Chromecast devices
int esp_cast_get_chromecast_devices_for_spotify(char devices[][64], int max_devices);
```

### Spotify Controller Functions

```c
// Authentication
bool spotify_controller_start_authentication(spotify_controller_handle_t handle);
bool spotify_controller_complete_authentication(spotify_controller_handle_t handle, const char* auth_code);
bool spotify_controller_is_authenticated(spotify_controller_handle_t handle);

// Connection
bool spotify_controller_connect(spotify_controller_handle_t handle);
bool spotify_controller_is_connected(spotify_controller_handle_t handle);

// Playback Control
bool spotify_controller_play(spotify_controller_handle_t handle, const char* uri);
bool spotify_controller_pause(spotify_controller_handle_t handle);
bool spotify_controller_next_track(spotify_controller_handle_t handle);
bool spotify_controller_previous_track(spotify_controller_handle_t handle);

// Content Access
bool spotify_controller_get_playlists(spotify_controller_handle_t handle);
bool spotify_controller_get_playlist_tracks(spotify_controller_handle_t handle, const char* playlist_id);
bool spotify_controller_search_tracks(spotify_controller_handle_t handle, const char* query, int limit);
```

## Configuration Options

### OAuth Scopes

The default scopes include:
- `user-read-playback-state`: Read current playback state
- `user-modify-playback-state`: Control playback
- `user-read-currently-playing`: Read currently playing track
- `playlist-read-private`: Access private playlists
- `playlist-read-collaborative`: Access collaborative playlists

### Network Configuration

Ensure your ESP32 has sufficient heap memory and network stack configuration:

```
CONFIG_FREERTOS_HZ=1000
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096
```

## Troubleshooting

### Common Issues

1. **Authentication Fails**
   - Check client ID and secret
   - Verify redirect URI matches Spotify app settings
   - Ensure WiFi is connected

2. **No Playlists Shown**
   - Check authentication status
   - Verify network connectivity
   - Check Spotify API rate limits

3. **Casting Fails**
   - Ensure Chromecast devices are on same network
   - Check Chromecast device discovery
   - Verify track URI format

### Debug Logging

Enable debug logging for troubleshooting:

```c
esp_log_level_set("spotify_controller", ESP_LOG_DEBUG);
esp_log_level_set("spotify_auth", ESP_LOG_DEBUG);
esp_log_level_set("spotify_api_client", ESP_LOG_DEBUG);
esp_log_level_set("spotify_gui_manager", ESP_LOG_DEBUG);
```

## Security Considerations

1. **Client Secret**: Store securely, consider using PKCE flow without client secret
2. **Token Storage**: Tokens are stored in NVS (encrypted if NVS encryption is enabled)
3. **Network Security**: Use secure WiFi networks
4. **Access Control**: Limit Spotify app permissions to required scopes

## Limitations

1. **Preview URLs**: Full track streaming requires Spotify Premium and additional integration
2. **Device Control**: Limited to devices with Spotify Connect support
3. **Offline Mode**: Requires internet connection for all operations
4. **Rate Limits**: Subject to Spotify API rate limits

## Future Enhancements

Potential improvements for future versions:
1. **Full Audio Streaming**: Direct audio streaming to Chromecast
2. **Offline Caching**: Cache track metadata for offline browsing
3. **Advanced Search**: Search filters and categories
4. **Queue Management**: Advanced playlist and queue management
5. **Multi-device Sync**: Synchronize across multiple ESP32 devices

## Support

For issues and questions:
1. Check the debug logs
2. Verify Spotify app configuration
3. Test with minimal example code
4. Check network connectivity and device discovery

## Example Code

See `components/spotify_controller/example_usage.cpp` for a complete example implementation.
