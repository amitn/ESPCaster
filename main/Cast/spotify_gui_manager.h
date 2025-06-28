#pragma once

#include "spotify_controller_wrapper.h"
#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Spotify GUI Manager - Handles LVGL interface for Spotify integration
 * 
 * This component provides a graphical user interface for Spotify functionality
 * using LVGL, including authentication, playlist browsing, search, and playback control.
 */

/**
 * @brief Spotify GUI Manager configuration
 */
typedef struct {
    lv_obj_t *parent;                           // Parent object for Spotify GUI elements
    spotify_controller_handle_t controller;     // Spotify controller handle
    bool show_status_bar;                       // Show Spotify status bar
    bool show_auth_button;                      // Show authentication button
    bool show_search_bar;                       // Show search functionality
} spotify_gui_config_t;

/**
 * @brief Spotify GUI screen types
 */
typedef enum {
    SPOTIFY_GUI_SCREEN_CONFIG,      // Configuration screen (for entering credentials)
    SPOTIFY_GUI_SCREEN_AUTH,        // Authentication screen
    SPOTIFY_GUI_SCREEN_PLAYLISTS,   // Playlists browser
    SPOTIFY_GUI_SCREEN_TRACKS,      // Track list
    SPOTIFY_GUI_SCREEN_PLAYER,      // Now playing screen
    SPOTIFY_GUI_SCREEN_SEARCH       // Search screen
} spotify_gui_screen_t;

/**
 * @brief Initialize Spotify GUI Manager
 * 
 * @param config GUI configuration
 * @return esp_err_t ESP_OK on success
 */
esp_err_t spotify_gui_manager_init(const spotify_gui_config_t *config);

/**
 * @brief Deinitialize Spotify GUI Manager
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t spotify_gui_manager_deinit(void);

/**
 * @brief Create Spotify management interface
 * 
 * Creates the main Spotify management interface with authentication and controls
 * 
 * @param parent Parent LVGL object
 * @return lv_obj_t* Created container object
 */
lv_obj_t *spotify_gui_create_interface(lv_obj_t *parent);

/**
 * @brief Show configuration screen
 *
 * Displays input fields for Spotify credentials
 */
void spotify_gui_show_config_screen(void);

/**
 * @brief Show authentication screen
 *
 * Displays QR code and instructions for user authentication
 */
void spotify_gui_show_auth_screen(void);

/**
 * @brief Show playlists screen
 * 
 * @param playlists Array of playlists
 * @param playlist_count Number of playlists
 */
void spotify_gui_show_playlists(const spotify_playlist_info_t *playlists, size_t playlist_count);

/**
 * @brief Show tracks screen
 * 
 * @param tracks Array of tracks
 * @param track_count Number of tracks
 * @param title Screen title (e.g., playlist name)
 */
void spotify_gui_show_tracks(const spotify_track_info_t *tracks, size_t track_count, const char *title);

/**
 * @brief Show now playing screen
 * 
 * @param playback_state Current playback state
 */
void spotify_gui_show_player(const spotify_playback_state_t *playback_state);

/**
 * @brief Show search screen
 */
void spotify_gui_show_search_screen(void);

/**
 * @brief Update authentication status
 * 
 * @param state Authentication state
 * @param message Status message
 */
void spotify_gui_update_auth_status(spotify_auth_state_t state, const char *message);

/**
 * @brief Update connection status
 * 
 * @param state Connection state
 * @param message Status message
 */
void spotify_gui_update_connection_status(spotify_connection_state_t state, const char *message);

/**
 * @brief Update playback state display
 * 
 * @param playback_state Current playback state
 */
void spotify_gui_update_playback_state(const spotify_playback_state_t *playback_state);

/**
 * @brief Show error message
 * 
 * @param error_message Error message to display
 */
void spotify_gui_show_error(const char *error_message);

/**
 * @brief Hide error message
 */
void spotify_gui_hide_error(void);

/**
 * @brief Navigate to specific screen
 * 
 * @param screen Target screen
 */
void spotify_gui_navigate_to_screen(spotify_gui_screen_t screen);

/**
 * @brief Get current screen
 * 
 * @return spotify_gui_screen_t Current screen
 */
spotify_gui_screen_t spotify_gui_get_current_screen(void);

/**
 * @brief Show loading indicator
 * 
 * @param message Loading message
 */
void spotify_gui_show_loading(const char *message);

/**
 * @brief Hide loading indicator
 */
void spotify_gui_hide_loading(void);

/**
 * @brief Create QR code for authentication URL
 * 
 * @param parent Parent object
 * @param url Authentication URL
 * @return lv_obj_t* QR code object
 */
lv_obj_t *spotify_gui_create_qr_code(lv_obj_t *parent, const char *url);

/**
 * @brief Create playback controls
 * 
 * @param parent Parent object
 * @return lv_obj_t* Controls container
 */
lv_obj_t *spotify_gui_create_playback_controls(lv_obj_t *parent);

/**
 * @brief Create volume control
 * 
 * @param parent Parent object
 * @return lv_obj_t* Volume control container
 */
lv_obj_t *spotify_gui_create_volume_control(lv_obj_t *parent);

/**
 * @brief Create search bar
 * 
 * @param parent Parent object
 * @return lv_obj_t* Search bar object
 */
lv_obj_t *spotify_gui_create_search_bar(lv_obj_t *parent);

/**
 * @brief Create playlist item
 * 
 * @param parent Parent list object
 * @param playlist Playlist information
 * @return lv_obj_t* Playlist item object
 */
lv_obj_t *spotify_gui_create_playlist_item(lv_obj_t *parent, const spotify_playlist_info_t *playlist);

/**
 * @brief Create track item
 * 
 * @param parent Parent list object
 * @param track Track information
 * @return lv_obj_t* Track item object
 */
lv_obj_t *spotify_gui_create_track_item(lv_obj_t *parent, const spotify_track_info_t *track);

/**
 * @brief Get status bar object
 * 
 * @return lv_obj_t* Status bar object or NULL
 */
lv_obj_t *spotify_gui_get_status_bar(void);

/**
 * @brief Set status bar position
 * 
 * @param align Alignment
 * @param x_offset X offset
 * @param y_offset Y offset
 */
void spotify_gui_set_status_bar_position(lv_align_t align, int32_t x_offset, int32_t y_offset);

/**
 * @brief Set controller handle for the GUI manager
 * 
 * @param controller Controller handle
 */
void spotify_gui_set_controller_handle(spotify_controller_handle_t controller);

/**
 * @brief Get current controller handle
 * 
 * @return spotify_controller_handle_t Current controller handle or NULL
 */
spotify_controller_handle_t spotify_gui_get_controller_handle(void);

/**
 * @brief Handle authentication completion
 * 
 * Called when authentication is completed successfully
 */
void spotify_gui_handle_auth_complete(void);

/**
 * @brief Handle authentication failure
 * 
 * @param error_message Error message
 */
void spotify_gui_handle_auth_failure(const char *error_message);

/**
 * @brief Refresh current screen
 *
 * Refreshes the currently displayed screen with latest data
 */
void spotify_gui_refresh_current_screen(void);

/**
 * @brief Show Chromecast device selection for casting
 *
 * @param track_uri Spotify track URI to cast
 */
void spotify_gui_show_chromecast_selection(const char *track_uri);

/**
 * @brief Cast track to selected Chromecast device
 *
 * @param device_name Chromecast device name
 * @param track_uri Spotify track URI
 */
void spotify_gui_cast_to_chromecast(const char *device_name, const char *track_uri);

#ifdef __cplusplus
}
#endif
