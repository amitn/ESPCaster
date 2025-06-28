#include "spotify_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "spotify_example";

// Example Spotify app credentials (replace with your own)
static const char* SPOTIFY_CLIENT_ID = "your_spotify_client_id_here";
static const char* SPOTIFY_CLIENT_SECRET = "your_spotify_client_secret_here";
static const char* SPOTIFY_REDIRECT_URI = "http://localhost:8888/callback";

// Global controller instance
static std::unique_ptr<SpotifyController> spotify_controller;

// Callback functions
void auth_state_callback(SpotifyAuthState state) {
    ESP_LOGI(TAG, "Auth state changed: %d", (int)state);
    
    switch (state) {
        case SpotifyAuthState::AUTHENTICATED:
            ESP_LOGI(TAG, "Successfully authenticated with Spotify!");
            break;
        case SpotifyAuthState::TOKEN_EXPIRED:
            ESP_LOGW(TAG, "Spotify token expired, attempting refresh...");
            break;
        case SpotifyAuthState::ERROR_STATE:
            ESP_LOGE(TAG, "Spotify authentication error");
            break;
        default:
            break;
    }
}

void connection_state_callback(SpotifyConnectionState state) {
    ESP_LOGI(TAG, "Connection state changed: %d", (int)state);
    
    switch (state) {
        case SpotifyConnectionState::CONNECTED:
            ESP_LOGI(TAG, "Connected to Spotify API!");
            // Start getting user data
            spotify_controller->get_user_playlists();
            break;
        case SpotifyConnectionState::ERROR_STATE:
            ESP_LOGE(TAG, "Spotify connection error");
            break;
        default:
            break;
    }
}

void playback_state_callback(const SpotifyPlaybackState& state) {
    ESP_LOGI(TAG, "Playback state updated:");
    ESP_LOGI(TAG, "  Playing: %s", state.is_playing ? "Yes" : "No");
    ESP_LOGI(TAG, "  Track: %s by %s", state.current_track.name.c_str(), state.current_track.artist.c_str());
    ESP_LOGI(TAG, "  Progress: %d/%d ms", state.progress_ms, state.current_track.duration_ms);
    ESP_LOGI(TAG, "  Volume: %d%%", state.volume_percent);
}

void playlists_callback(const std::vector<SpotifyPlaylist>& playlists) {
    ESP_LOGI(TAG, "Received %d playlists:", playlists.size());
    
    for (size_t i = 0; i < playlists.size() && i < 5; i++) {
        ESP_LOGI(TAG, "  %d. %s (%d tracks) by %s", 
                 i + 1, playlists[i].name.c_str(), playlists[i].track_count, playlists[i].owner.c_str());
    }
    
    // Get tracks from first playlist as example
    if (!playlists.empty()) {
        ESP_LOGI(TAG, "Getting tracks from first playlist: %s", playlists[0].name.c_str());
        spotify_controller->get_playlist_tracks(playlists[0].id);
    }
}

void tracks_callback(const std::vector<SpotifyTrack>& tracks) {
    ESP_LOGI(TAG, "Received %d tracks:", tracks.size());
    
    for (size_t i = 0; i < tracks.size() && i < 10; i++) {
        ESP_LOGI(TAG, "  %d. %s by %s (%d:%02d)", 
                 i + 1, tracks[i].name.c_str(), tracks[i].artist.c_str(),
                 tracks[i].duration_ms / 60000, (tracks[i].duration_ms / 1000) % 60);
    }
    
    // Play first track as example
    if (!tracks.empty()) {
        ESP_LOGI(TAG, "Playing first track: %s", tracks[0].name.c_str());
        spotify_controller->play(tracks[0].uri);
    }
}

void devices_callback(const std::vector<SpotifyDevice>& devices) {
    ESP_LOGI(TAG, "Received %d devices:", devices.size());
    
    for (const auto& device : devices) {
        ESP_LOGI(TAG, "  %s (%s) - %s, Volume: %d%%", 
                 device.name.c_str(), device.type.c_str(),
                 device.is_active ? "Active" : "Inactive",
                 device.volume_percent);
    }
}

void error_callback(const std::string& error) {
    ESP_LOGE(TAG, "Spotify error: %s", error.c_str());
}

// Example task to demonstrate Spotify integration
void spotify_example_task(void *parameter) {
    ESP_LOGI(TAG, "Starting Spotify integration example");
    
    // Create and initialize Spotify controller
    spotify_controller = std::make_unique<SpotifyController>();
    
    if (!spotify_controller->initialize(SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET, SPOTIFY_REDIRECT_URI)) {
        ESP_LOGE(TAG, "Failed to initialize Spotify controller");
        vTaskDelete(NULL);
        return;
    }
    
    // Set up callbacks
    spotify_controller->set_auth_state_callback(auth_state_callback);
    spotify_controller->set_connection_state_callback(connection_state_callback);
    spotify_controller->set_playback_state_callback(playback_state_callback);
    spotify_controller->set_playlists_callback(playlists_callback);
    spotify_controller->set_tracks_callback(tracks_callback);
    spotify_controller->set_devices_callback(devices_callback);
    spotify_controller->set_error_callback(error_callback);
    
    // Start authentication process
    ESP_LOGI(TAG, "Starting authentication...");
    if (!spotify_controller->start_authentication()) {
        ESP_LOGE(TAG, "Failed to start authentication");
        vTaskDelete(NULL);
        return;
    }
    
    // Get authentication URL for user
    std::string auth_url = spotify_controller->get_auth_url();
    ESP_LOGI(TAG, "Please visit this URL to authenticate:");
    ESP_LOGI(TAG, "%s", auth_url.c_str());
    
    // Main loop - run periodic tasks
    while (true) {
        spotify_controller->run_periodic_tasks();
        
        // Example: Get current playback state every 10 seconds if connected
        static int counter = 0;
        if (spotify_controller->is_connected() && (counter % 100) == 0) {
            spotify_controller->get_current_playback_state();
        }
        counter++;
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Example function to demonstrate search functionality
void spotify_search_example() {
    if (!spotify_controller || !spotify_controller->is_connected()) {
        ESP_LOGE(TAG, "Spotify not connected");
        return;
    }
    
    ESP_LOGI(TAG, "Searching for tracks...");
    spotify_controller->search_tracks("The Beatles", 10);
}

// Example function to demonstrate playback control
void spotify_playback_control_example() {
    if (!spotify_controller || !spotify_controller->is_connected()) {
        ESP_LOGE(TAG, "Spotify not connected");
        return;
    }
    
    ESP_LOGI(TAG, "Demonstrating playback controls...");
    
    // Get current state
    spotify_controller->get_current_playback_state();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Pause
    spotify_controller->pause();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Resume
    spotify_controller->play();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Next track
    spotify_controller->next_track();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Set volume
    spotify_controller->set_volume(50);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Enable shuffle
    spotify_controller->set_shuffle(true);
}

// Example function to demonstrate Chromecast integration
void spotify_chromecast_example() {
    if (!spotify_controller || !spotify_controller->is_connected()) {
        ESP_LOGE(TAG, "Spotify not connected");
        return;
    }
    
    ESP_LOGI(TAG, "Demonstrating Chromecast integration...");
    
    // Example track URI (replace with actual track)
    std::string track_uri = "spotify:track:4iV5W9uYEdYUVa79Axb7Rh"; // "Never Gonna Give You Up"
    std::string chromecast_ip = "192.168.1.100"; // Replace with actual Chromecast IP
    
    bool success = spotify_controller->cast_to_chromecast(chromecast_ip, track_uri);
    if (success) {
        ESP_LOGI(TAG, "Successfully initiated casting to Chromecast");
    } else {
        ESP_LOGE(TAG, "Failed to cast to Chromecast");
    }
}

// Function to start the example
extern "C" void start_spotify_example() {
    xTaskCreate(spotify_example_task, "spotify_example", 8192, NULL, 5, NULL);
}

// Function to simulate authentication completion (for testing)
extern "C" void complete_spotify_auth_example(const char* auth_code) {
    if (spotify_controller && auth_code) {
        ESP_LOGI(TAG, "Completing authentication with code: %s", auth_code);
        spotify_controller->complete_authentication(auth_code);
    }
}

// Function to run search example
extern "C" void run_spotify_search_example() {
    spotify_search_example();
}

// Function to run playback control example
extern "C" void run_spotify_playback_example() {
    spotify_playback_control_example();
}

// Function to run Chromecast example
extern "C" void run_spotify_chromecast_example() {
    spotify_chromecast_example();
}
