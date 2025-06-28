#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_http_client.h"

/**
 * SpotifyController - ESP-IDF C++ class for Spotify Web API integration
 * 
 * Features:
 * - OAuth2 authentication with PKCE
 * - Spotify Web API client
 * - Playback control
 * - Playlist and track management
 * - Integration with Chromecast for casting
 */

// Forward declarations
class SpotifyAuth;
class SpotifyApiClient;

/**
 * @brief Spotify track information
 */
struct SpotifyTrack {
    std::string id;
    std::string name;
    std::string artist;
    std::string album;
    std::string uri;
    int duration_ms;
    std::string preview_url;
    std::string image_url;
};

/**
 * @brief Spotify playlist information
 */
struct SpotifyPlaylist {
    std::string id;
    std::string name;
    std::string description;
    std::string uri;
    int track_count;
    std::string image_url;
    std::string owner;
};

/**
 * @brief Spotify playback state
 */
struct SpotifyPlaybackState {
    bool is_playing;
    int progress_ms;
    int volume_percent;
    bool shuffle_state;
    std::string repeat_state; // "off", "track", "context"
    SpotifyTrack current_track;
    std::string device_id;
    std::string device_name;
};

/**
 * @brief Spotify device information
 */
struct SpotifyDevice {
    std::string id;
    std::string name;
    std::string type;
    bool is_active;
    bool is_private_session;
    bool is_restricted;
    int volume_percent;
};

// Forward declaration - defined in spotify_auth.h
enum class SpotifyAuthState;

/**
 * @brief Connection state
 */
enum class SpotifyConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR_STATE
};

class SpotifyController {
public:
    // Callback function types
    using AuthStateCallback = std::function<void(SpotifyAuthState)>;
    using ConnectionStateCallback = std::function<void(SpotifyConnectionState)>;
    using PlaybackStateCallback = std::function<void(const SpotifyPlaybackState&)>;
    using PlaylistsCallback = std::function<void(const std::vector<SpotifyPlaylist>&)>;
    using TracksCallback = std::function<void(const std::vector<SpotifyTrack>&)>;
    using DevicesCallback = std::function<void(const std::vector<SpotifyDevice>&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

private:
    // Component instances
    std::unique_ptr<SpotifyAuth> auth_client;
    std::unique_ptr<SpotifyApiClient> api_client;

    // State management
    SpotifyAuthState auth_state;
    SpotifyConnectionState connection_state;
    SpotifyPlaybackState current_playback_state;
    std::vector<SpotifyPlaylist> user_playlists;
    std::vector<SpotifyDevice> available_devices;

    // Callbacks
    AuthStateCallback auth_state_callback;
    ConnectionStateCallback connection_state_callback;
    PlaybackStateCallback playback_state_callback;
    PlaylistsCallback playlists_callback;
    TracksCallback tracks_callback;
    DevicesCallback devices_callback;
    ErrorCallback error_callback;

    // Configuration
    std::string client_id;
    std::string client_secret;
    std::string redirect_uri;

    // Internal methods
    void handle_auth_state_change(SpotifyAuthState new_state);
    void handle_api_error(const std::string& error);
    void update_playback_state();
    void refresh_user_data();

    // Static callback functions for components
    static void auth_state_callback_wrapper(SpotifyAuthState state, void* user_data);
    static void api_response_callback_wrapper(const std::string& response, void* user_data);

public:
    SpotifyController();
    ~SpotifyController();

    // Initialization and configuration
    bool initialize(const std::string& client_id, const std::string& client_secret, 
                   const std::string& redirect_uri = "http://localhost:8888/callback");
    void deinitialize();

    // Authentication methods
    bool start_authentication();
    bool complete_authentication(const std::string& auth_code);
    bool refresh_token();
    void logout();
    bool is_authenticated() const;

    // Connection methods
    bool connect();
    void disconnect();
    bool is_connected() const;

    // Playback control methods
    bool play(const std::string& uri = "");
    bool pause();
    bool next_track();
    bool previous_track();
    bool seek_to_position(int position_ms);
    bool set_volume(int volume_percent);
    bool set_shuffle(bool shuffle);
    bool set_repeat(const std::string& repeat_state);

    // Device management
    bool get_available_devices();
    bool transfer_playback(const std::string& device_id);

    // Content methods
    bool get_user_playlists();
    bool get_playlist_tracks(const std::string& playlist_id);
    bool search_tracks(const std::string& query, int limit = 20);
    bool get_current_playback_state();

    // Casting integration
    bool cast_to_chromecast(const std::string& chromecast_ip, const std::string& track_uri);

    // Callback setters
    void set_auth_state_callback(AuthStateCallback callback) { auth_state_callback = callback; }
    void set_connection_state_callback(ConnectionStateCallback callback) { connection_state_callback = callback; }
    void set_playback_state_callback(PlaybackStateCallback callback) { playback_state_callback = callback; }
    void set_playlists_callback(PlaylistsCallback callback) { playlists_callback = callback; }
    void set_tracks_callback(TracksCallback callback) { tracks_callback = callback; }
    void set_devices_callback(DevicesCallback callback) { devices_callback = callback; }
    void set_error_callback(ErrorCallback callback) { error_callback = callback; }

    // State change handlers
    void handle_connection_state_change(SpotifyConnectionState new_state);

    // Getters
    SpotifyAuthState get_auth_state() const { return auth_state; }
    SpotifyConnectionState get_connection_state() const { return connection_state; }
    const SpotifyPlaybackState& get_playback_state() const { return current_playback_state; }
    const std::vector<SpotifyPlaylist>& get_playlists() const { return user_playlists; }
    const std::vector<SpotifyDevice>& get_devices() const { return available_devices; }

    // Utility methods
    std::string get_auth_url() const;
    bool is_token_valid() const;
    void run_periodic_tasks();
};
