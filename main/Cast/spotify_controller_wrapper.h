#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Spotify Controller Wrapper - C interface for Spotify integration
 * 
 * This wrapper provides a C interface for the C++ SpotifyController class,
 * allowing integration with the existing C-based ESP Cast application.
 */

/**
 * @brief Opaque handle for Spotify controller instance
 */
typedef void* spotify_controller_handle_t;

/**
 * @brief Spotify authentication state
 */
typedef enum {
    SPOTIFY_AUTH_NOT_AUTHENTICATED,
    SPOTIFY_AUTH_AUTHENTICATING,
    SPOTIFY_AUTH_AUTHENTICATED,
    SPOTIFY_AUTH_TOKEN_EXPIRED,
    SPOTIFY_AUTH_ERROR_STATE
} spotify_auth_state_t;

/**
 * @brief Spotify connection state
 */
typedef enum {
    SPOTIFY_CONNECTION_DISCONNECTED,
    SPOTIFY_CONNECTION_CONNECTING,
    SPOTIFY_CONNECTION_CONNECTED,
    SPOTIFY_CONNECTION_ERROR_STATE
} spotify_connection_state_t;

/**
 * @brief Spotify track information (C struct)
 */
typedef struct {
    char id[64];
    char name[256];
    char artist[256];
    char album[256];
    char uri[256];
    int duration_ms;
    char preview_url[512];
    char image_url[512];
} spotify_track_info_t;

/**
 * @brief Spotify playlist information (C struct)
 */
typedef struct {
    char id[64];
    char name[256];
    char description[512];
    char uri[256];
    int track_count;
    char image_url[512];
    char owner[128];
} spotify_playlist_info_t;

/**
 * @brief Spotify playback state (C struct)
 */
typedef struct {
    bool is_playing;
    int progress_ms;
    int volume_percent;
    bool shuffle_state;
    char repeat_state[16]; // "off", "track", "context"
    spotify_track_info_t current_track;
    char device_id[64];
    char device_name[256];
} spotify_playback_state_t;

/**
 * @brief Spotify device information (C struct)
 */
typedef struct {
    char id[64];
    char name[256];
    char type[64];
    bool is_active;
    bool is_private_session;
    bool is_restricted;
    int volume_percent;
} spotify_device_info_t;

/**
 * @brief Callback function types
 */
typedef void (*spotify_auth_state_callback_t)(spotify_auth_state_t state);
typedef void (*spotify_connection_state_callback_t)(spotify_connection_state_t state);
typedef void (*spotify_playback_state_callback_t)(const spotify_playback_state_t* state);
typedef void (*spotify_playlists_callback_t)(const spotify_playlist_info_t* playlists, size_t count);
typedef void (*spotify_tracks_callback_t)(const spotify_track_info_t* tracks, size_t count);
typedef void (*spotify_devices_callback_t)(const spotify_device_info_t* devices, size_t count);
typedef void (*spotify_error_callback_t)(const char* error_message);

/**
 * @brief Create Spotify controller instance
 * 
 * @return spotify_controller_handle_t Handle to controller instance or NULL on failure
 */
spotify_controller_handle_t spotify_controller_create(void);

/**
 * @brief Destroy Spotify controller instance
 * 
 * @param handle Controller handle
 */
void spotify_controller_destroy(spotify_controller_handle_t handle);

/**
 * @brief Initialize Spotify controller
 * 
 * @param handle Controller handle
 * @param client_id Spotify app client ID
 * @param client_secret Spotify app client secret (can be NULL for PKCE flow)
 * @param redirect_uri OAuth redirect URI
 * @return true on success, false on failure
 */
bool spotify_controller_initialize(spotify_controller_handle_t handle, 
                                 const char* client_id, 
                                 const char* client_secret,
                                 const char* redirect_uri);

/**
 * @brief Deinitialize Spotify controller
 * 
 * @param handle Controller handle
 */
void spotify_controller_deinitialize(spotify_controller_handle_t handle);

/**
 * @brief Start authentication process
 * 
 * @param handle Controller handle
 * @return true on success, false on failure
 */
bool spotify_controller_start_authentication(spotify_controller_handle_t handle);

/**
 * @brief Get authentication URL for user to visit
 * 
 * @param handle Controller handle
 * @param url_buffer Buffer to store URL
 * @param buffer_size Size of URL buffer
 * @return true on success, false on failure
 */
bool spotify_controller_get_auth_url(spotify_controller_handle_t handle, char* url_buffer, size_t buffer_size);

/**
 * @brief Complete authentication with authorization code
 * 
 * @param handle Controller handle
 * @param auth_code Authorization code from callback
 * @return true on success, false on failure
 */
bool spotify_controller_complete_authentication(spotify_controller_handle_t handle, const char* auth_code);

/**
 * @brief Check if authenticated
 * 
 * @param handle Controller handle
 * @return true if authenticated, false otherwise
 */
bool spotify_controller_is_authenticated(spotify_controller_handle_t handle);

/**
 * @brief Connect to Spotify
 * 
 * @param handle Controller handle
 * @return true on success, false on failure
 */
bool spotify_controller_connect(spotify_controller_handle_t handle);

/**
 * @brief Disconnect from Spotify
 * 
 * @param handle Controller handle
 */
void spotify_controller_disconnect(spotify_controller_handle_t handle);

/**
 * @brief Check if connected
 * 
 * @param handle Controller handle
 * @return true if connected, false otherwise
 */
bool spotify_controller_is_connected(spotify_controller_handle_t handle);

/**
 * @brief Start/resume playback
 * 
 * @param handle Controller handle
 * @param uri Optional URI to play (NULL for current context)
 * @return true on success, false on failure
 */
bool spotify_controller_play(spotify_controller_handle_t handle, const char* uri);

/**
 * @brief Pause playback
 * 
 * @param handle Controller handle
 * @return true on success, false on failure
 */
bool spotify_controller_pause(spotify_controller_handle_t handle);

/**
 * @brief Skip to next track
 * 
 * @param handle Controller handle
 * @return true on success, false on failure
 */
bool spotify_controller_next_track(spotify_controller_handle_t handle);

/**
 * @brief Skip to previous track
 * 
 * @param handle Controller handle
 * @return true on success, false on failure
 */
bool spotify_controller_previous_track(spotify_controller_handle_t handle);

/**
 * @brief Set playback volume
 * 
 * @param handle Controller handle
 * @param volume_percent Volume percentage (0-100)
 * @return true on success, false on failure
 */
bool spotify_controller_set_volume(spotify_controller_handle_t handle, int volume_percent);

/**
 * @brief Get user playlists
 * 
 * @param handle Controller handle
 * @return true on success, false on failure
 */
bool spotify_controller_get_playlists(spotify_controller_handle_t handle);

/**
 * @brief Get playlist tracks
 * 
 * @param handle Controller handle
 * @param playlist_id Playlist ID
 * @return true on success, false on failure
 */
bool spotify_controller_get_playlist_tracks(spotify_controller_handle_t handle, const char* playlist_id);

/**
 * @brief Search for tracks
 * 
 * @param handle Controller handle
 * @param query Search query
 * @param limit Maximum number of results
 * @return true on success, false on failure
 */
bool spotify_controller_search_tracks(spotify_controller_handle_t handle, const char* query, int limit);

/**
 * @brief Get current playback state
 * 
 * @param handle Controller handle
 * @return true on success, false on failure
 */
bool spotify_controller_get_playback_state(spotify_controller_handle_t handle);

/**
 * @brief Get available devices
 * 
 * @param handle Controller handle
 * @return true on success, false on failure
 */
bool spotify_controller_get_devices(spotify_controller_handle_t handle);

/**
 * @brief Cast to Chromecast device
 * 
 * @param handle Controller handle
 * @param chromecast_ip Chromecast device IP address
 * @param track_uri Spotify track URI to cast
 * @return true on success, false on failure
 */
bool spotify_controller_cast_to_chromecast(spotify_controller_handle_t handle, 
                                          const char* chromecast_ip, 
                                          const char* track_uri);

/**
 * @brief Set callback functions
 */
void spotify_controller_set_auth_state_callback(spotify_controller_handle_t handle, 
                                               spotify_auth_state_callback_t callback);
void spotify_controller_set_connection_state_callback(spotify_controller_handle_t handle, 
                                                     spotify_connection_state_callback_t callback);
void spotify_controller_set_playback_state_callback(spotify_controller_handle_t handle, 
                                                   spotify_playback_state_callback_t callback);
void spotify_controller_set_playlists_callback(spotify_controller_handle_t handle, 
                                              spotify_playlists_callback_t callback);
void spotify_controller_set_tracks_callback(spotify_controller_handle_t handle, 
                                           spotify_tracks_callback_t callback);
void spotify_controller_set_devices_callback(spotify_controller_handle_t handle, 
                                            spotify_devices_callback_t callback);
void spotify_controller_set_error_callback(spotify_controller_handle_t handle, 
                                          spotify_error_callback_t callback);

/**
 * @brief Get current state
 */
spotify_auth_state_t spotify_controller_get_auth_state(spotify_controller_handle_t handle);
spotify_connection_state_t spotify_controller_get_connection_state(spotify_controller_handle_t handle);

/**
 * @brief Run periodic tasks (call from main loop)
 * 
 * @param handle Controller handle
 */
void spotify_controller_run_periodic_tasks(spotify_controller_handle_t handle);

#ifdef __cplusplus
}
#endif
