#include "spotify_gui_manager.h"
#include "esp_cast.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "spotify_gui_manager";

// Forward declarations for callback functions
static void track_play_button_cb(lv_event_t *e);
static void track_cast_button_cb(lv_event_t *e);
static void chromecast_device_button_cb(lv_event_t *e);
static void close_modal_button_cb(lv_event_t *e);

// GUI state
typedef struct {
    bool initialized;
    lv_obj_t *status_bar;
    lv_obj_t *auth_button;
    lv_obj_t *main_container;
    lv_obj_t *current_screen;
    lv_obj_t *loading_spinner;
    lv_obj_t *error_modal;
    spotify_controller_handle_t controller_handle;
    spotify_gui_screen_t current_screen_type;
    
    // Screen containers
    lv_obj_t *auth_screen;
    lv_obj_t *playlists_screen;
    lv_obj_t *tracks_screen;
    lv_obj_t *player_screen;
    lv_obj_t *search_screen;
    
    // Current data
    const spotify_playlist_info_t *current_playlists;
    size_t current_playlist_count;
    const spotify_track_info_t *current_tracks;
    size_t current_track_count;
} spotify_gui_state_t;

static spotify_gui_state_t g_gui_state = {0};

// Forward declarations
static void auth_button_cb(lv_event_t *e);
static void playlist_button_cb(lv_event_t *e);
static void track_button_cb(lv_event_t *e);
static void play_button_cb(lv_event_t *e);
static void pause_button_cb(lv_event_t *e);
static void next_button_cb(lv_event_t *e);
static void prev_button_cb(lv_event_t *e);
static void search_button_cb(lv_event_t *e);
static void back_button_cb(lv_event_t *e);

// Callback handlers for Spotify controller
static void spotify_auth_state_callback(spotify_auth_state_t state);
static void spotify_connection_state_callback(spotify_connection_state_t state);
static void spotify_playback_state_callback(const spotify_playback_state_t* state);
static void spotify_playlists_callback(const spotify_playlist_info_t* playlists, size_t count);
static void spotify_tracks_callback(const spotify_track_info_t* tracks, size_t count);
static void spotify_devices_callback(const spotify_device_info_t* devices, size_t count);
static void spotify_error_callback(const char* error_message);

esp_err_t spotify_gui_manager_init(const spotify_gui_config_t *config) {
    if (g_gui_state.initialized) {
        ESP_LOGW(TAG, "Spotify GUI Manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing Spotify GUI Manager");

    // Store controller handle
    if (config && config->controller) {
        g_gui_state.controller_handle = config->controller;
        
        // Set up controller callbacks
        spotify_controller_set_auth_state_callback(g_gui_state.controller_handle, spotify_auth_state_callback);
        spotify_controller_set_connection_state_callback(g_gui_state.controller_handle, spotify_connection_state_callback);
        spotify_controller_set_playback_state_callback(g_gui_state.controller_handle, spotify_playback_state_callback);
        spotify_controller_set_playlists_callback(g_gui_state.controller_handle, spotify_playlists_callback);
        spotify_controller_set_tracks_callback(g_gui_state.controller_handle, spotify_tracks_callback);
        spotify_controller_set_devices_callback(g_gui_state.controller_handle, spotify_devices_callback);
        spotify_controller_set_error_callback(g_gui_state.controller_handle, spotify_error_callback);
    }

    // Initialize state
    g_gui_state.current_screen_type = SPOTIFY_GUI_SCREEN_AUTH;
    g_gui_state.current_playlists = NULL;
    g_gui_state.current_playlist_count = 0;
    g_gui_state.current_tracks = NULL;
    g_gui_state.current_track_count = 0;

    g_gui_state.initialized = true;
    ESP_LOGI(TAG, "Spotify GUI Manager initialized successfully");
    return ESP_OK;
}

esp_err_t spotify_gui_manager_deinit(void) {
    if (!g_gui_state.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing Spotify GUI Manager");

    // Clean up GUI objects
    if (g_gui_state.main_container) {
        lv_obj_del(g_gui_state.main_container);
        g_gui_state.main_container = NULL;
    }

    // Reset state
    memset(&g_gui_state, 0, sizeof(g_gui_state));
    
    ESP_LOGI(TAG, "Spotify GUI Manager deinitialized");
    return ESP_OK;
}

lv_obj_t *spotify_gui_create_interface(lv_obj_t *parent) {
    if (!parent) {
        ESP_LOGE(TAG, "Parent object is NULL");
        return NULL;
    }

    ESP_LOGI(TAG, "Creating Spotify GUI interface");

    // Create main container
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    lv_obj_center(container);

    g_gui_state.main_container = container;

    // Create status bar
    g_gui_state.status_bar = lv_label_create(container);
    lv_obj_align(g_gui_state.status_bar, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_label_set_text(g_gui_state.status_bar, "Spotify: Not authenticated");

    // Show initial authentication screen
    spotify_gui_show_auth_screen();

    return container;
}

void spotify_gui_show_auth_screen(void) {
    ESP_LOGI(TAG, "Showing authentication screen");
    
    // Clear current screen
    if (g_gui_state.current_screen) {
        lv_obj_del(g_gui_state.current_screen);
        g_gui_state.current_screen = NULL;
    }
    
    // Create auth screen container
    g_gui_state.auth_screen = lv_obj_create(g_gui_state.main_container);
    lv_obj_set_size(g_gui_state.auth_screen, lv_pct(90), lv_pct(80));
    lv_obj_center(g_gui_state.auth_screen);
    
    g_gui_state.current_screen = g_gui_state.auth_screen;
    g_gui_state.current_screen_type = SPOTIFY_GUI_SCREEN_AUTH;
    
    // Create title
    lv_obj_t *title = lv_label_create(g_gui_state.auth_screen);
    lv_label_set_text(title, "Spotify Authentication");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Create authentication button
    g_gui_state.auth_button = lv_btn_create(g_gui_state.auth_screen);
    lv_obj_set_size(g_gui_state.auth_button, 200, 50);
    lv_obj_center(g_gui_state.auth_button);
    lv_obj_add_event_cb(g_gui_state.auth_button, auth_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *auth_label = lv_label_create(g_gui_state.auth_button);
    lv_label_set_text(auth_label, "Authenticate");
    lv_obj_center(auth_label);
    
    // Create instructions
    lv_obj_t *instructions = lv_label_create(g_gui_state.auth_screen);
    lv_label_set_text(instructions, "Click to start Spotify authentication.\nYou will need to visit a URL in your browser.");
    lv_label_set_long_mode(instructions, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(instructions, lv_pct(80));
    lv_obj_align(instructions, LV_ALIGN_BOTTOM_MID, 0, -20);
}

void spotify_gui_show_playlists(const spotify_playlist_info_t *playlists, size_t playlist_count) {
    if (!playlists || playlist_count == 0) {
        ESP_LOGW(TAG, "No playlists to display");
        return;
    }

    ESP_LOGI(TAG, "Displaying %d playlists", playlist_count);
    
    // Store playlist data
    g_gui_state.current_playlists = playlists;
    g_gui_state.current_playlist_count = playlist_count;
    
    // Clear current screen
    if (g_gui_state.current_screen) {
        lv_obj_del(g_gui_state.current_screen);
        g_gui_state.current_screen = NULL;
    }
    
    // Create playlists screen
    g_gui_state.playlists_screen = lv_obj_create(g_gui_state.main_container);
    lv_obj_set_size(g_gui_state.playlists_screen, lv_pct(90), lv_pct(80));
    lv_obj_center(g_gui_state.playlists_screen);
    
    g_gui_state.current_screen = g_gui_state.playlists_screen;
    g_gui_state.current_screen_type = SPOTIFY_GUI_SCREEN_PLAYLISTS;
    
    // Create title
    lv_obj_t *title = lv_label_create(g_gui_state.playlists_screen);
    lv_label_set_text(title, "Your Playlists");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Create playlist list
    lv_obj_t *list = lv_list_create(g_gui_state.playlists_screen);
    lv_obj_set_size(list, lv_pct(90), lv_pct(70));
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 10);
    
    // Add playlists to list
    for (size_t i = 0; i < playlist_count; i++) {
        spotify_gui_create_playlist_item(list, &playlists[i]);
    }
}

void spotify_gui_show_tracks(const spotify_track_info_t *tracks, size_t track_count, const char *title) {
    if (!tracks || track_count == 0) {
        ESP_LOGW(TAG, "No tracks to display");
        return;
    }

    ESP_LOGI(TAG, "Displaying %d tracks", track_count);
    
    // Store track data
    g_gui_state.current_tracks = tracks;
    g_gui_state.current_track_count = track_count;
    
    // Clear current screen
    if (g_gui_state.current_screen) {
        lv_obj_del(g_gui_state.current_screen);
        g_gui_state.current_screen = NULL;
    }
    
    // Create tracks screen
    g_gui_state.tracks_screen = lv_obj_create(g_gui_state.main_container);
    lv_obj_set_size(g_gui_state.tracks_screen, lv_pct(90), lv_pct(80));
    lv_obj_center(g_gui_state.tracks_screen);
    
    g_gui_state.current_screen = g_gui_state.tracks_screen;
    g_gui_state.current_screen_type = SPOTIFY_GUI_SCREEN_TRACKS;
    
    // Create title
    lv_obj_t *title_label = lv_label_create(g_gui_state.tracks_screen);
    lv_label_set_text(title_label, title ? title : "Tracks");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // Create back button
    lv_obj_t *back_btn = lv_btn_create(g_gui_state.tracks_screen);
    lv_obj_set_size(back_btn, 60, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back_btn, back_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);
    
    // Create track list
    lv_obj_t *list = lv_list_create(g_gui_state.tracks_screen);
    lv_obj_set_size(list, lv_pct(90), lv_pct(70));
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 10);
    
    // Add tracks to list
    for (size_t i = 0; i < track_count; i++) {
        spotify_gui_create_track_item(list, &tracks[i]);
    }
}

lv_obj_t *spotify_gui_create_playlist_item(lv_obj_t *parent, const spotify_playlist_info_t *playlist) {
    if (!parent || !playlist) return NULL;
    
    // Create button with playlist name and track count
    char btn_text[300];
    snprintf(btn_text, sizeof(btn_text), "%s (%d tracks)", 
             playlist->name, playlist->track_count);
    
    lv_obj_t *btn = lv_list_add_btn(parent, LV_SYMBOL_AUDIO, btn_text);
    
    // Store playlist data in button user data
    spotify_playlist_info_t *playlist_data = malloc(sizeof(spotify_playlist_info_t));
    if (playlist_data) {
        memcpy(playlist_data, playlist, sizeof(spotify_playlist_info_t));
        lv_obj_set_user_data(btn, playlist_data);
        lv_obj_add_event_cb(btn, playlist_button_cb, LV_EVENT_CLICKED, NULL);
    }
    
    return btn;
}

lv_obj_t *spotify_gui_create_track_item(lv_obj_t *parent, const spotify_track_info_t *track) {
    if (!parent || !track) return NULL;
    
    // Create button with track name and artist
    char btn_text[600];  // Increased buffer size to accommodate longer strings
    snprintf(btn_text, sizeof(btn_text), "%s - %s",
             track->name, track->artist);
    
    lv_obj_t *btn = lv_list_add_btn(parent, LV_SYMBOL_PLAY, btn_text);
    
    // Store track data in button user data
    spotify_track_info_t *track_data = malloc(sizeof(spotify_track_info_t));
    if (track_data) {
        memcpy(track_data, track, sizeof(spotify_track_info_t));
        lv_obj_set_user_data(btn, track_data);
        lv_obj_add_event_cb(btn, track_button_cb, LV_EVENT_CLICKED, NULL);
    }
    
    return btn;
}

// Callback handlers for Spotify controller
static void spotify_auth_state_callback(spotify_auth_state_t state) {
    ESP_LOGI(TAG, "Auth state changed: %d", state);

    switch (state) {
        case SPOTIFY_AUTH_AUTHENTICATED:
            spotify_gui_hide_loading();
            spotify_gui_update_auth_status(state, "Successfully authenticated");
            // Navigate to playlists screen
            spotify_gui_navigate_to_screen(SPOTIFY_GUI_SCREEN_PLAYLISTS);
            break;
        case SPOTIFY_AUTH_ERROR_STATE:
            spotify_gui_hide_loading();
            spotify_gui_show_error("Authentication failed");
            break;
        default:
            spotify_gui_update_auth_status(state, NULL);
            break;
    }
}

static void spotify_connection_state_callback(spotify_connection_state_t state) {
    ESP_LOGI(TAG, "Connection state changed: %d", state);
    spotify_gui_update_connection_status(state, NULL);

    if (state == SPOTIFY_CONNECTION_CONNECTED) {
        // Request initial data
        if (g_gui_state.controller_handle) {
            spotify_controller_get_playlists(g_gui_state.controller_handle);
        }
    }
}

static void spotify_playback_state_callback(const spotify_playback_state_t* state) {
    if (!state) return;

    ESP_LOGI(TAG, "Playback state updated: %s - %s",
             state->current_track.name,
             state->is_playing ? "Playing" : "Paused");

    // Update GUI if on player screen
    if (g_gui_state.current_screen_type == SPOTIFY_GUI_SCREEN_PLAYER) {
        spotify_gui_show_player(state);
    }
}

static void spotify_playlists_callback(const spotify_playlist_info_t* playlists, size_t count) {
    ESP_LOGI(TAG, "Received %d playlists", count);

    spotify_gui_hide_loading();

    if (playlists && count > 0) {
        spotify_gui_show_playlists(playlists, count);
    } else {
        spotify_gui_show_error("No playlists found");
    }
}

static void spotify_tracks_callback(const spotify_track_info_t* tracks, size_t count) {
    ESP_LOGI(TAG, "Received %d tracks", count);

    spotify_gui_hide_loading();

    if (tracks && count > 0) {
        spotify_gui_show_tracks(tracks, count, "Playlist Tracks");
    } else {
        spotify_gui_show_error("No tracks found");
    }
}

static void spotify_devices_callback(const spotify_device_info_t* devices, size_t count) {
    ESP_LOGI(TAG, "Received %d devices", count);
    // Device handling can be implemented later
}

static void spotify_error_callback(const char* error_message) {
    ESP_LOGE(TAG, "Spotify error: %s", error_message);
    spotify_gui_hide_loading();
    spotify_gui_show_error(error_message);
}

// Event handlers
static void auth_button_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Authentication button clicked");

    if (!g_gui_state.controller_handle) {
        spotify_gui_show_error("Controller not initialized");
        return;
    }

    // Get authentication URL
    char auth_url[512];
    if (spotify_controller_get_auth_url(g_gui_state.controller_handle, auth_url, sizeof(auth_url))) {
        ESP_LOGI(TAG, "Authentication URL: %s", auth_url);

        // Show instructions to user
        spotify_gui_show_loading("Please visit the authentication URL in your browser");

        // Start authentication process
        spotify_controller_start_authentication(g_gui_state.controller_handle);
    } else {
        spotify_gui_show_error("Failed to get authentication URL");
    }
}

static void playlist_button_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    spotify_playlist_info_t *playlist = (spotify_playlist_info_t*)lv_obj_get_user_data(btn);

    if (!playlist || !g_gui_state.controller_handle) {
        spotify_gui_show_error("Invalid playlist or controller");
        return;
    }

    ESP_LOGI(TAG, "Playlist clicked: %s", playlist->name);

    // Show loading and get playlist tracks
    spotify_gui_show_loading("Loading playlist tracks...");
    spotify_controller_get_playlist_tracks(g_gui_state.controller_handle, playlist->id);
}

static void track_button_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    spotify_track_info_t *track = (spotify_track_info_t*)lv_obj_get_user_data(btn);

    if (!track || !g_gui_state.controller_handle) {
        spotify_gui_show_error("Invalid track or controller");
        return;
    }

    ESP_LOGI(TAG, "Track clicked: %s", track->name);

    // Create action selection modal
    lv_obj_t *modal = lv_obj_create(lv_scr_act());
    lv_obj_set_size(modal, 250, 150);
    lv_obj_center(modal);
    lv_obj_add_flag(modal, LV_OBJ_FLAG_FLOATING);

    // Add title
    lv_obj_t *title = lv_label_create(modal);
    lv_label_set_text(title, "Select Action");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Create action buttons
    lv_obj_t *play_btn = lv_btn_create(modal);
    lv_obj_set_size(play_btn, 200, 40);
    lv_obj_align(play_btn, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *play_label = lv_label_create(play_btn);
    lv_label_set_text(play_label, "Play on Spotify");
    lv_obj_center(play_label);

    // Store track data for play button
    spotify_track_info_t *play_track_data = malloc(sizeof(spotify_track_info_t));
    if (play_track_data) {
        memcpy(play_track_data, track, sizeof(spotify_track_info_t));
        lv_obj_set_user_data(play_btn, play_track_data);
        lv_obj_add_event_cb(play_btn, track_play_button_cb, LV_EVENT_CLICKED, NULL);
    }

    // Cast button
    lv_obj_t *cast_btn = lv_btn_create(modal);
    lv_obj_set_size(cast_btn, 200, 40);
    lv_obj_align(cast_btn, LV_ALIGN_CENTER, 0, 20);

    lv_obj_t *cast_label = lv_label_create(cast_btn);
    lv_label_set_text(cast_label, "Cast to Chromecast");
    lv_obj_center(cast_label);

    // Store track data for cast button
    spotify_track_info_t *cast_track_data = malloc(sizeof(spotify_track_info_t));
    if (cast_track_data) {
        memcpy(cast_track_data, track, sizeof(spotify_track_info_t));
        lv_obj_set_user_data(cast_btn, cast_track_data);
        lv_obj_add_event_cb(cast_btn, track_cast_button_cb, LV_EVENT_CLICKED, NULL);
    }
}

static void back_button_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Back button clicked");

    // Navigate back to playlists
    spotify_gui_navigate_to_screen(SPOTIFY_GUI_SCREEN_PLAYLISTS);
}

// Placeholder implementations for remaining functions
static void play_button_cb(lv_event_t *e) {
    if (g_gui_state.controller_handle) {
        spotify_controller_play(g_gui_state.controller_handle, NULL);
    }
}

static void pause_button_cb(lv_event_t *e) {
    if (g_gui_state.controller_handle) {
        spotify_controller_pause(g_gui_state.controller_handle);
    }
}

static void next_button_cb(lv_event_t *e) {
    if (g_gui_state.controller_handle) {
        spotify_controller_next_track(g_gui_state.controller_handle);
    }
}

static void prev_button_cb(lv_event_t *e) {
    if (g_gui_state.controller_handle) {
        spotify_controller_previous_track(g_gui_state.controller_handle);
    }
}

static void search_button_cb(lv_event_t *e) {
    // Search functionality to be implemented
    ESP_LOGI(TAG, "Search button clicked - not implemented yet");
}

static void track_play_button_cb(lv_event_t *e) {
    spotify_track_info_t *track = (spotify_track_info_t*)lv_obj_get_user_data(lv_event_get_target(e));
    if (track && g_gui_state.controller_handle) {
        spotify_controller_play(g_gui_state.controller_handle, track->uri);
    }
    // Close modal
    lv_obj_t *modal = lv_obj_get_parent(lv_event_get_target(e));
    lv_obj_del(modal);
    free(track);
}

static void track_cast_button_cb(lv_event_t *e) {
    spotify_track_info_t *track = (spotify_track_info_t*)lv_obj_get_user_data(lv_event_get_target(e));
    if (track) {
        // Show Chromecast device selection
        spotify_gui_show_chromecast_selection(track->uri);
    }
    // Close modal
    lv_obj_t *modal = lv_obj_get_parent(lv_event_get_target(e));
    lv_obj_del(modal);
    free(track);
}

static void chromecast_device_button_cb(lv_event_t *e) {
    char *data = (char*)lv_obj_get_user_data(lv_event_get_target(e));
    if (data) {
        char *separator = strchr(data, '|');
        if (separator) {
            *separator = '\0';
            char *track_uri = data;
            char *device_name = separator + 1;

            ESP_LOGI(TAG, "Casting %s to %s", track_uri, device_name);
            spotify_gui_cast_to_chromecast(device_name, track_uri);

            // Close modal
            lv_obj_t *modal = lv_obj_get_parent(lv_obj_get_parent(lv_event_get_target(e)));
            lv_obj_del(modal);
        }
        free(data);
    }
}

static void close_modal_button_cb(lv_event_t *e) {
    lv_obj_t *modal = lv_obj_get_parent(lv_event_get_target(e));
    lv_obj_del(modal);
}

// Utility functions
void spotify_gui_update_auth_status(spotify_auth_state_t state, const char *message) {
    if (!g_gui_state.status_bar) return;

    char status_text[128];
    const char* state_str = "Unknown";

    switch (state) {
        case SPOTIFY_AUTH_NOT_AUTHENTICATED:
            state_str = "Not authenticated";
            break;
        case SPOTIFY_AUTH_AUTHENTICATING:
            state_str = "Authenticating...";
            break;
        case SPOTIFY_AUTH_AUTHENTICATED:
            state_str = "Authenticated";
            break;
        case SPOTIFY_AUTH_TOKEN_EXPIRED:
            state_str = "Token expired";
            break;
        case SPOTIFY_AUTH_ERROR_STATE:
            state_str = "Authentication error";
            break;
    }

    if (message) {
        snprintf(status_text, sizeof(status_text), "Spotify: %s - %s", state_str, message);
    } else {
        snprintf(status_text, sizeof(status_text), "Spotify: %s", state_str);
    }

    lv_label_set_text(g_gui_state.status_bar, status_text);
    ESP_LOGI(TAG, "Updated auth status: %s", status_text);
}

void spotify_gui_update_connection_status(spotify_connection_state_t state, const char *message) {
    if (!g_gui_state.status_bar) return;

    char status_text[128];
    const char* state_str = "Unknown";

    switch (state) {
        case SPOTIFY_CONNECTION_DISCONNECTED:
            state_str = "Disconnected";
            break;
        case SPOTIFY_CONNECTION_CONNECTING:
            state_str = "Connecting...";
            break;
        case SPOTIFY_CONNECTION_CONNECTED:
            state_str = "Connected";
            break;
        case SPOTIFY_CONNECTION_ERROR_STATE:
            state_str = "Connection error";
            break;
    }

    if (message) {
        snprintf(status_text, sizeof(status_text), "Spotify: %s - %s", state_str, message);
    } else {
        snprintf(status_text, sizeof(status_text), "Spotify: %s", state_str);
    }

    lv_label_set_text(g_gui_state.status_bar, status_text);
    ESP_LOGI(TAG, "Updated connection status: %s", status_text);
}

void spotify_gui_show_error(const char *error_message) {
    if (!error_message || !g_gui_state.main_container) return;

    ESP_LOGE(TAG, "Showing error: %s", error_message);

    // Create simple error display in status bar for now
    char error_text[128];
    snprintf(error_text, sizeof(error_text), "Error: %s", error_message);
    lv_label_set_text(g_gui_state.status_bar, error_text);
}

void spotify_gui_hide_error(void) {
    // Error is shown in status bar, so just update status
    spotify_gui_update_auth_status(SPOTIFY_AUTH_NOT_AUTHENTICATED, NULL);
}

void spotify_gui_show_loading(const char *message) {
    if (!g_gui_state.status_bar) return;

    char loading_text[128];
    snprintf(loading_text, sizeof(loading_text), "Loading: %s", message ? message : "Please wait...");
    lv_label_set_text(g_gui_state.status_bar, loading_text);
    ESP_LOGI(TAG, "Showing loading: %s", message ? message : "Loading...");
}

void spotify_gui_hide_loading(void) {
    // Loading is shown in status bar, so just update status
    spotify_gui_update_auth_status(SPOTIFY_AUTH_NOT_AUTHENTICATED, NULL);
}

void spotify_gui_navigate_to_screen(spotify_gui_screen_t screen) {
    switch (screen) {
        case SPOTIFY_GUI_SCREEN_AUTH:
            spotify_gui_show_auth_screen();
            break;
        case SPOTIFY_GUI_SCREEN_PLAYLISTS:
            if (g_gui_state.current_playlists && g_gui_state.current_playlist_count > 0) {
                spotify_gui_show_playlists(g_gui_state.current_playlists, g_gui_state.current_playlist_count);
            } else {
                // Request playlists
                if (g_gui_state.controller_handle) {
                    spotify_controller_get_playlists(g_gui_state.controller_handle);
                }
            }
            break;
        default:
            ESP_LOGW(TAG, "Unknown screen type: %d", screen);
            break;
    }
}

spotify_gui_screen_t spotify_gui_get_current_screen(void) {
    return g_gui_state.current_screen_type;
}

lv_obj_t *spotify_gui_get_status_bar(void) {
    return g_gui_state.status_bar;
}

void spotify_gui_set_controller_handle(spotify_controller_handle_t controller) {
    g_gui_state.controller_handle = controller;
}

spotify_controller_handle_t spotify_gui_get_controller_handle(void) {
    return g_gui_state.controller_handle;
}

// Placeholder implementations for functions not yet implemented
void spotify_gui_show_player(const spotify_playback_state_t *playback_state) {
    ESP_LOGI(TAG, "Show player screen - not implemented yet");
}

void spotify_gui_show_search_screen(void) {
    ESP_LOGI(TAG, "Show search screen - not implemented yet");
}

void spotify_gui_update_playback_state(const spotify_playback_state_t *playback_state) {
    ESP_LOGI(TAG, "Update playback state - not implemented yet");
}

lv_obj_t *spotify_gui_create_qr_code(lv_obj_t *parent, const char *url) {
    ESP_LOGI(TAG, "Create QR code - not implemented yet");
    return NULL;
}

lv_obj_t *spotify_gui_create_playback_controls(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Create playback controls - not implemented yet");
    return NULL;
}

lv_obj_t *spotify_gui_create_volume_control(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Create volume control - not implemented yet");
    return NULL;
}

lv_obj_t *spotify_gui_create_search_bar(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Create search bar - not implemented yet");
    return NULL;
}

void spotify_gui_set_status_bar_position(lv_align_t align, int32_t x_offset, int32_t y_offset) {
    if (g_gui_state.status_bar) {
        lv_obj_align(g_gui_state.status_bar, align, x_offset, y_offset);
    }
}

void spotify_gui_handle_auth_complete(void) {
    ESP_LOGI(TAG, "Authentication completed");
    spotify_gui_navigate_to_screen(SPOTIFY_GUI_SCREEN_PLAYLISTS);
}

void spotify_gui_handle_auth_failure(const char *error_message) {
    ESP_LOGE(TAG, "Authentication failed: %s", error_message);
    spotify_gui_show_error(error_message);
}

void spotify_gui_refresh_current_screen(void) {
    spotify_gui_navigate_to_screen(g_gui_state.current_screen_type);
}

void spotify_gui_show_chromecast_selection(const char *track_uri) {
    if (!track_uri || !g_gui_state.main_container) {
        ESP_LOGE(TAG, "Invalid parameters for Chromecast selection");
        return;
    }

    ESP_LOGI(TAG, "Showing Chromecast device selection for track: %s", track_uri);

    // Create modal dialog for device selection
    lv_obj_t *modal = lv_obj_create(lv_scr_act());
    lv_obj_set_size(modal, 300, 200);
    lv_obj_center(modal);
    lv_obj_add_flag(modal, LV_OBJ_FLAG_FLOATING);

    // Add title
    lv_obj_t *title = lv_label_create(modal);
    lv_label_set_text(title, "Select Chromecast Device");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Get available Chromecast devices
    char devices[5][64];
    int device_count = esp_cast_get_chromecast_devices_for_spotify_strings(devices, 5);

    if (device_count == 0) {
        lv_obj_t *no_devices = lv_label_create(modal);
        lv_label_set_text(no_devices, "No Chromecast devices found");
        lv_obj_center(no_devices);
    } else {
        // Create device list
        lv_obj_t *list = lv_list_create(modal);
        lv_obj_set_size(list, 250, 120);
        lv_obj_align(list, LV_ALIGN_CENTER, 0, 0);

        for (int i = 0; i < device_count; i++) {
            lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_AUDIO, devices[i]);

            // Store track URI and device name for the callback
            char *user_data = malloc(strlen(track_uri) + strlen(devices[i]) + 2);
            if (user_data) {
                sprintf(user_data, "%s|%s", track_uri, devices[i]);
                lv_obj_set_user_data(btn, user_data);
                lv_obj_add_event_cb(btn, chromecast_device_button_cb, LV_EVENT_CLICKED, NULL);
            }
        }
    }

    // Add close button
    lv_obj_t *close_btn = lv_btn_create(modal);
    lv_obj_set_size(close_btn, 60, 30);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_add_event_cb(close_btn, close_modal_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "Close");
    lv_obj_center(close_label);
}

void spotify_gui_cast_to_chromecast(const char *device_name, const char *track_uri) {
    if (!device_name || !track_uri) {
        ESP_LOGE(TAG, "Invalid parameters for casting");
        return;
    }

    ESP_LOGI(TAG, "Casting track %s to device %s", track_uri, device_name);

    // Show loading
    spotify_gui_show_loading("Casting to Chromecast...");

    // Perform the cast
    bool success = esp_cast_spotify_to_chromecast(device_name, track_uri);

    // Hide loading
    spotify_gui_hide_loading();

    if (success) {
        char success_msg[128];
        snprintf(success_msg, sizeof(success_msg), "Casting to %s", device_name);
        spotify_gui_update_connection_status(SPOTIFY_CONNECTION_CONNECTED, success_msg);
    } else {
        spotify_gui_show_error("Failed to cast to Chromecast device");
    }
}
