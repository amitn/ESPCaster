#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "spotify_controller.h"

/**
 * SpotifyApiClient - HTTP client for Spotify Web API
 * 
 * Provides methods to interact with Spotify Web API endpoints including
 * playback control, playlist management, search, and user data.
 */

/**
 * @brief HTTP response structure
 */
struct SpotifyApiResponse {
    int status_code;
    std::string body;
    bool success;
    std::string error_message;
};

/**
 * @brief API request configuration
 */
struct SpotifyApiRequest {
    std::string method;     // GET, POST, PUT, DELETE
    std::string endpoint;   // API endpoint path
    std::string body;       // Request body (for POST/PUT)
    bool requires_auth;     // Whether request needs authorization header
};

class SpotifyApiClient {
public:
    // Callback function types
    using ResponseCallback = std::function<void(const SpotifyApiResponse&, void*)>;
    using PlaybackStateCallback = std::function<void(const SpotifyPlaybackState&, void*)>;
    using PlaylistsCallback = std::function<void(const std::vector<SpotifyPlaylist>&, void*)>;
    using TracksCallback = std::function<void(const std::vector<SpotifyTrack>&, void*)>;
    using DevicesCallback = std::function<void(const std::vector<SpotifyDevice>&, void*)>;
    using ErrorCallback = std::function<void(const std::string&, void*)>;

private:
    // HTTP client configuration
    esp_http_client_handle_t http_client;
    std::string access_token;
    std::string base_url;
    
    // Callback storage
    ResponseCallback response_callback;
    PlaybackStateCallback playback_callback;
    PlaylistsCallback playlists_callback;
    TracksCallback tracks_callback;
    DevicesCallback devices_callback;
    ErrorCallback error_callback;
    void* callback_user_data;
    
    // Rate limiting
    time_t last_request_time;
    int requests_per_second_limit;
    
    // Internal methods
    SpotifyApiResponse make_request(const SpotifyApiRequest& request);
    bool setup_http_client();
    void cleanup_http_client();
    std::string build_url(const std::string& endpoint);
    bool add_auth_header();
    bool check_rate_limit();
    void handle_api_error(int status_code, const std::string& response_body);
    
    // JSON parsing helpers
    SpotifyPlaybackState parse_playback_state(cJSON* json);
    std::vector<SpotifyPlaylist> parse_playlists(cJSON* json);
    std::vector<SpotifyTrack> parse_tracks(cJSON* json);
    std::vector<SpotifyDevice> parse_devices(cJSON* json);
    SpotifyTrack parse_track(cJSON* track_json);
    
    // HTTP client event handler
    static esp_err_t http_event_handler(esp_http_client_event_t* evt);
    
public:
    SpotifyApiClient();
    ~SpotifyApiClient();
    
    // Initialization
    bool initialize(const std::string& access_token);
    void deinitialize();
    void set_access_token(const std::string& token);
    
    // Player API methods
    bool get_playback_state();
    bool start_resume_playback(const std::string& device_id = "", const std::string& context_uri = "");
    bool pause_playback(const std::string& device_id = "");
    bool skip_to_next(const std::string& device_id = "");
    bool skip_to_previous(const std::string& device_id = "");
    bool seek_to_position(int position_ms, const std::string& device_id = "");
    bool set_repeat_mode(const std::string& state, const std::string& device_id = "");
    bool set_playback_volume(int volume_percent, const std::string& device_id = "");
    bool toggle_playback_shuffle(bool state, const std::string& device_id = "");
    bool transfer_playback(const std::string& device_id, bool play = false);
    bool add_to_queue(const std::string& uri, const std::string& device_id = "");
    
    // Device API methods
    bool get_available_devices();
    
    // Playlist API methods
    bool get_user_playlists(const std::string& user_id = "me", int limit = 20, int offset = 0);
    bool get_playlist_tracks(const std::string& playlist_id, int limit = 100, int offset = 0);
    bool get_featured_playlists(int limit = 20, int offset = 0);
    
    // Search API methods
    bool search(const std::string& query, const std::string& type = "track", int limit = 20, int offset = 0);
    
    // User API methods
    bool get_current_user_profile();
    bool get_user_top_tracks(const std::string& time_range = "medium_term", int limit = 20, int offset = 0);
    bool get_user_top_artists(const std::string& time_range = "medium_term", int limit = 20, int offset = 0);
    
    // Track API methods
    bool get_track(const std::string& track_id);
    bool get_several_tracks(const std::vector<std::string>& track_ids);
    bool get_audio_features(const std::string& track_id);
    
    // Callback setters
    void set_response_callback(ResponseCallback callback, void* user_data = nullptr) {
        response_callback = callback;
        callback_user_data = user_data;
    }
    void set_playback_callback(PlaybackStateCallback callback, void* user_data = nullptr) {
        playback_callback = callback;
        callback_user_data = user_data;
    }
    void set_playlists_callback(PlaylistsCallback callback, void* user_data = nullptr) {
        playlists_callback = callback;
        callback_user_data = user_data;
    }
    void set_tracks_callback(TracksCallback callback, void* user_data = nullptr) {
        tracks_callback = callback;
        callback_user_data = user_data;
    }
    void set_devices_callback(DevicesCallback callback, void* user_data = nullptr) {
        devices_callback = callback;
        callback_user_data = user_data;
    }
    void set_error_callback(ErrorCallback callback, void* user_data = nullptr) {
        error_callback = callback;
        callback_user_data = user_data;
    }
    
    // Utility methods
    bool is_initialized() const { return http_client != nullptr; }
    std::string get_last_error() const;
    
    // Constants
    static constexpr const char* SPOTIFY_API_BASE_URL = "https://api.spotify.com/v1";
    static constexpr int DEFAULT_RATE_LIMIT = 100; // requests per second
    static constexpr int HTTP_TIMEOUT_MS = 10000;
};
