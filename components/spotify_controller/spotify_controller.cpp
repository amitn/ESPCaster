#include "spotify_controller.h"
#include "spotify_auth.h"
#include "spotify_api_client.h"
#include "esp_log.h"
#include <memory>

static const char *TAG = "spotify_controller";

SpotifyController::SpotifyController()
    : auth_state(SpotifyAuthState::NOT_AUTHENTICATED)
    , connection_state(SpotifyConnectionState::DISCONNECTED)
    , auth_state_callback(nullptr)
    , connection_state_callback(nullptr)
    , playback_state_callback(nullptr)
    , playlists_callback(nullptr)
    , tracks_callback(nullptr)
    , devices_callback(nullptr)
    , error_callback(nullptr) {
    
    // Initialize playback state
    memset(&current_playback_state, 0, sizeof(current_playback_state));
}

SpotifyController::~SpotifyController() {
    deinitialize();
}

bool SpotifyController::initialize(const std::string& client_id, 
                                 const std::string& client_secret,
                                 const std::string& redirect_uri) {
    ESP_LOGI(TAG, "Initializing Spotify controller");
    
    this->client_id = client_id;
    this->client_secret = client_secret;
    this->redirect_uri = redirect_uri;
    
    // Create authentication client
    auth_client = std::make_unique<SpotifyAuth>();
    if (!auth_client) {
        ESP_LOGE(TAG, "Failed to create authentication client");
        return false;
    }
    
    // Initialize authentication
    if (!auth_client->initialize(client_id, redirect_uri)) {
        ESP_LOGE(TAG, "Failed to initialize authentication");
        return false;
    }
    
    // Set up authentication callbacks
    auth_client->set_auth_state_callback(auth_state_callback_wrapper, this);
    
    // Create API client
    api_client = std::make_unique<SpotifyApiClient>();
    if (!api_client) {
        ESP_LOGE(TAG, "Failed to create API client");
        return false;
    }
    
    // Set up API client callbacks
    api_client->set_playback_callback([this](const SpotifyPlaybackState& state, void* user_data) {
        this->current_playback_state = state;
        if (this->playback_state_callback) {
            this->playback_state_callback(state);
        }
    });
    
    api_client->set_playlists_callback([this](const std::vector<SpotifyPlaylist>& playlists, void* user_data) {
        this->user_playlists = playlists;
        if (this->playlists_callback) {
            this->playlists_callback(playlists);
        }
    });
    
    api_client->set_tracks_callback([this](const std::vector<SpotifyTrack>& tracks, void* user_data) {
        if (this->tracks_callback) {
            this->tracks_callback(tracks);
        }
    });
    
    api_client->set_devices_callback([this](const std::vector<SpotifyDevice>& devices, void* user_data) {
        this->available_devices = devices;
        if (this->devices_callback) {
            this->devices_callback(devices);
        }
    });
    
    api_client->set_error_callback([this](const std::string& error, void* user_data) {
        this->handle_api_error(error);
    });
    
    ESP_LOGI(TAG, "Spotify controller initialized successfully");
    return true;
}

void SpotifyController::deinitialize() {
    ESP_LOGI(TAG, "Deinitializing Spotify controller");
    
    disconnect();
    
    if (api_client) {
        api_client->deinitialize();
        api_client.reset();
    }
    
    if (auth_client) {
        auth_client->deinitialize();
        auth_client.reset();
    }
    
    // Clear state
    auth_state = SpotifyAuthState::NOT_AUTHENTICATED;
    connection_state = SpotifyConnectionState::DISCONNECTED;
    user_playlists.clear();
    available_devices.clear();
    memset(&current_playback_state, 0, sizeof(current_playback_state));
}

bool SpotifyController::start_authentication() {
    if (!auth_client) {
        ESP_LOGE(TAG, "Authentication client not initialized");
        return false;
    }
    
    ESP_LOGI(TAG, "Starting authentication process");
    return true; // The actual authentication URL will be provided via get_auth_url()
}

bool SpotifyController::complete_authentication(const std::string& auth_code) {
    if (!auth_client) {
        ESP_LOGE(TAG, "Authentication client not initialized");
        return false;
    }
    
    ESP_LOGI(TAG, "Completing authentication with authorization code");
    return auth_client->handle_authorization_response(auth_code, ""); // State verification handled internally
}

bool SpotifyController::refresh_token() {
    if (!auth_client) {
        ESP_LOGE(TAG, "Authentication client not initialized");
        return false;
    }
    
    return auth_client->refresh_token();
}

void SpotifyController::logout() {
    ESP_LOGI(TAG, "Logging out");
    
    disconnect();
    
    if (auth_client) {
        auth_client->logout();
    }
}

bool SpotifyController::is_authenticated() const {
    return auth_client && auth_client->is_authenticated();
}

bool SpotifyController::connect() {
    if (!is_authenticated()) {
        ESP_LOGE(TAG, "Not authenticated, cannot connect");
        return false;
    }
    
    if (!api_client) {
        ESP_LOGE(TAG, "API client not initialized");
        return false;
    }
    
    ESP_LOGI(TAG, "Connecting to Spotify API");
    
    handle_connection_state_change(SpotifyConnectionState::CONNECTING);
    
    // Initialize API client with access token
    std::string access_token = auth_client->get_access_token();
    if (!api_client->initialize(access_token)) {
        ESP_LOGE(TAG, "Failed to initialize API client");
        handle_connection_state_change(SpotifyConnectionState::ERROR_STATE);
        return false;
    }
    
    handle_connection_state_change(SpotifyConnectionState::CONNECTED);
    
    // Refresh initial data
    refresh_user_data();
    
    return true;
}

void SpotifyController::disconnect() {
    ESP_LOGI(TAG, "Disconnecting from Spotify API");
    
    if (api_client) {
        api_client->deinitialize();
    }
    
    handle_connection_state_change(SpotifyConnectionState::DISCONNECTED);
}

bool SpotifyController::is_connected() const {
    return connection_state == SpotifyConnectionState::CONNECTED;
}

// Playback control methods
bool SpotifyController::play(const std::string& uri) {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Spotify API");
        return false;
    }
    
    return api_client->start_resume_playback("", uri);
}

bool SpotifyController::pause() {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Spotify API");
        return false;
    }
    
    return api_client->pause_playback();
}

bool SpotifyController::next_track() {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Spotify API");
        return false;
    }
    
    return api_client->skip_to_next();
}

bool SpotifyController::previous_track() {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Spotify API");
        return false;
    }
    
    return api_client->skip_to_previous();
}

bool SpotifyController::seek_to_position(int position_ms) {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Spotify API");
        return false;
    }
    
    return api_client->seek_to_position(position_ms);
}

bool SpotifyController::set_volume(int volume_percent) {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Spotify API");
        return false;
    }
    
    return api_client->set_playback_volume(volume_percent);
}

bool SpotifyController::set_shuffle(bool shuffle) {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Spotify API");
        return false;
    }
    
    return api_client->toggle_playback_shuffle(shuffle);
}

bool SpotifyController::set_repeat(const std::string& repeat_state) {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Spotify API");
        return false;
    }
    
    return api_client->set_repeat_mode(repeat_state);
}

// Device management
bool SpotifyController::get_available_devices() {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Spotify API");
        return false;
    }
    
    return api_client->get_available_devices();
}

bool SpotifyController::transfer_playback(const std::string& device_id) {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Spotify API");
        return false;
    }
    
    return api_client->transfer_playback(device_id, true);
}

// Content methods
bool SpotifyController::get_user_playlists() {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Spotify API");
        return false;
    }
    
    return api_client->get_user_playlists();
}

bool SpotifyController::get_playlist_tracks(const std::string& playlist_id) {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Spotify API");
        return false;
    }
    
    return api_client->get_playlist_tracks(playlist_id);
}

bool SpotifyController::search_tracks(const std::string& query, int limit) {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Spotify API");
        return false;
    }
    
    return api_client->search(query, "track", limit);
}

bool SpotifyController::get_current_playback_state() {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Spotify API");
        return false;
    }
    
    return api_client->get_playback_state();
}

// Casting integration
bool SpotifyController::cast_to_chromecast(const std::string& chromecast_ip, const std::string& track_uri) {
    ESP_LOGI(TAG, "Attempting to cast Spotify track to Chromecast: %s", chromecast_ip.c_str());

    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Spotify API");
        return false;
    }

    if (chromecast_ip.empty() || track_uri.empty()) {
        ESP_LOGE(TAG, "Invalid parameters for casting");
        return false;
    }

    // For now, we'll implement a basic approach:
    // 1. Get track information from Spotify
    // 2. Use the preview URL if available
    // 3. Send to Chromecast via HTTP streaming

    // First, try to get track details
    std::string track_id = track_uri;
    if (track_uri.find("spotify:track:") == 0) {
        track_id = track_uri.substr(14); // Remove "spotify:track:" prefix
    }

    // For this implementation, we'll use a simplified approach
    // In a full implementation, you would:
    // 1. Use Spotify Connect API to transfer playback to a Chromecast-enabled device
    // 2. Or stream the preview URL to Chromecast
    // 3. Or integrate with a media server that can handle Spotify URLs

    ESP_LOGI(TAG, "Casting functionality requires additional integration with Chromecast media streaming");
    ESP_LOGI(TAG, "Track URI: %s, Chromecast IP: %s", track_uri.c_str(), chromecast_ip.c_str());

    // For demonstration, we'll return true but log that full implementation is needed
    ESP_LOGW(TAG, "Casting simulation - full implementation requires media streaming setup");

    return true; // Return true for demo purposes
}

// Utility methods
std::string SpotifyController::get_auth_url() const {
    if (!auth_client) {
        ESP_LOGE(TAG, "Authentication client not initialized");
        return "";
    }

    return auth_client->get_authorization_url();
}

bool SpotifyController::is_token_valid() const {
    return auth_client && auth_client->is_token_valid();
}

void SpotifyController::run_periodic_tasks() {
    // Run authentication periodic tasks
    if (auth_client) {
        auth_client->run_periodic_tasks();
    }

    // Update playback state periodically if connected
    if (is_connected()) {
        static time_t last_update = 0;
        time_t now;
        time(&now);

        // Update every 5 seconds
        if (now - last_update >= 5) {
            get_current_playback_state();
            last_update = now;
        }
    }
}

// Internal methods
void SpotifyController::handle_auth_state_change(SpotifyAuthState new_state) {
    if (auth_state != new_state) {
        auth_state = new_state;
        ESP_LOGI(TAG, "Authentication state changed to: %d", (int)new_state);

        if (auth_state_callback) {
            auth_state_callback(new_state);
        }

        // Handle state transitions
        if (new_state == SpotifyAuthState::AUTHENTICATED) {
            // Automatically connect when authenticated
            connect();
        } else if (new_state == SpotifyAuthState::NOT_AUTHENTICATED ||
                   new_state == SpotifyAuthState::ERROR_STATE) {
            // Disconnect when authentication is lost
            disconnect();
        }
    }
}

void SpotifyController::handle_connection_state_change(SpotifyConnectionState new_state) {
    if (connection_state != new_state) {
        connection_state = new_state;
        ESP_LOGI(TAG, "Connection state changed to: %d", (int)new_state);

        if (connection_state_callback) {
            connection_state_callback(new_state);
        }
    }
}

void SpotifyController::handle_api_error(const std::string& error) {
    ESP_LOGE(TAG, "API error: %s", error.c_str());

    if (error_callback) {
        error_callback(error);
    }

    // Handle specific error cases
    if (error.find("401") != std::string::npos || error.find("Unauthorized") != std::string::npos) {
        // Token expired or invalid
        ESP_LOGI(TAG, "Token appears to be invalid, attempting refresh");
        if (!refresh_token()) {
            handle_auth_state_change(SpotifyAuthState::TOKEN_EXPIRED);
        }
    }
}

void SpotifyController::update_playback_state() {
    if (is_connected()) {
        get_current_playback_state();
    }
}

void SpotifyController::refresh_user_data() {
    if (!is_connected()) {
        return;
    }

    ESP_LOGI(TAG, "Refreshing user data");

    // Get user playlists
    get_user_playlists();

    // Get available devices
    get_available_devices();

    // Get current playback state
    get_current_playback_state();
}

// Static callback functions
void SpotifyController::auth_state_callback_wrapper(SpotifyAuthState state, void* user_data) {
    SpotifyController* controller = static_cast<SpotifyController*>(user_data);
    if (controller) {
        controller->handle_auth_state_change(state);
    }
}

void SpotifyController::api_response_callback_wrapper(const std::string& response, void* user_data) {
    SpotifyController* controller = static_cast<SpotifyController*>(user_data);
    if (controller) {
        // Handle generic API responses if needed
        ESP_LOGD(TAG, "API response received: %d bytes", response.length());
    }
}
