#pragma once

/**
 * Spotify Configuration Template for ESPCast
 * 
 * Copy this file to your main directory and rename it to spotify_config.h
 * Fill in your Spotify app credentials below.
 * 
 * To get Spotify credentials:
 * 1. Go to https://developer.spotify.com/dashboard
 * 2. Create a new app
 * 3. Note your Client ID and Client Secret
 * 4. Add redirect URI: http://localhost:8888/callback
 */

// Your Spotify App Client ID (required)
#define SPOTIFY_CLIENT_ID "your_spotify_client_id_here"

// Your Spotify App Client Secret (optional for PKCE flow)
#define SPOTIFY_CLIENT_SECRET "your_spotify_client_secret_here"

// OAuth Redirect URI (must match your Spotify app settings)
#define SPOTIFY_REDIRECT_URI "http://localhost:8888/callback"

// OAuth Scopes (customize as needed)
#define SPOTIFY_SCOPES "user-read-playback-state user-modify-playback-state user-read-currently-playing playlist-read-private playlist-read-collaborative"

// Example usage in your main.c:
/*
#include "spotify_config.h"

void app_main(void) {
    // ... other initialization ...
    
    // Initialize Spotify
    bool success = esp_cast_spotify_init(
        SPOTIFY_CLIENT_ID,
        SPOTIFY_CLIENT_SECRET,
        SPOTIFY_REDIRECT_URI
    );
    
    if (success) {
        ESP_LOGI("APP", "Spotify initialized successfully");
    } else {
        ESP_LOGE("APP", "Failed to initialize Spotify");
    }
    
    // ... rest of app ...
}
*/

// Security Notes:
// 1. Keep your client secret secure
// 2. Consider using PKCE flow (set client secret to NULL)
// 3. Use environment variables or secure storage in production
// 4. Limit OAuth scopes to only what you need

// Testing Configuration:
// For testing, you can use these settings:
// - Client ID: Get from Spotify Developer Dashboard
// - Client Secret: Optional, can be NULL for PKCE
// - Redirect URI: http://localhost:8888/callback
// - Scopes: Use the default scopes above

// Production Configuration:
// For production deployment:
// 1. Store credentials securely (NVS, environment variables)
// 2. Use HTTPS redirect URIs if possible
// 3. Implement proper error handling
// 4. Monitor API usage and rate limits
// 5. Handle token refresh automatically
