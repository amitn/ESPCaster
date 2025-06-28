#pragma once

#include "esp_wifi.h"
#include "lvgl.h"
#include "spotify_controller_wrapper.h"
#include "chromecast_discovery_wrapper.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize ESP Cast GUI
 *
 * Creates the main GUI interface including WiFi management and mDNS controls
 */
void esp_cast_gui_init(void);

/**
 * @brief Initialize WiFi station mode
 *
 * Initializes WiFi manager and mDNS for Chromecast discovery
 */
void esp_cast_wifi_init_sta(void);

/**
 * @brief Main ESP Cast loop
 *
 * Handles periodic tasks like mDNS result processing
 */
void esp_cast_loop(void);

/**
 * @brief Show WiFi scan results (legacy compatibility)
 *
 * @param aps Array of WiFi access points
 * @param ap_num Number of access points
 */
void esp_cast_show_wifi_list(wifi_ap_record_t *aps, uint16_t ap_num);

/**
 * @brief Update WiFi status display (legacy compatibility)
 *
 * @param ssid Connected network SSID
 * @param ip IP address
 */
void gui_update_wifi_status(const char *ssid, const char *ip);

/**
 * @brief Test WiFi auto-connect functionality
 *
 * Attempts to connect to saved WiFi credentials
 */
void esp_cast_test_wifi_auto_connect(void);

/**
 * @brief Test default WiFi credentials functionality
 *
 * Tests connecting to default WiFi credentials from project configuration
 */
void esp_cast_test_default_wifi(void);

/**
 * @brief Initialize Spotify controller
 *
 * Creates and initializes the Spotify controller with client credentials
 *
 * @param client_id Spotify app client ID
 * @param client_secret Spotify app client secret (optional for PKCE)
 * @param redirect_uri OAuth redirect URI
 * @return true on success, false on failure
 */
bool esp_cast_spotify_init(const char* client_id, const char* client_secret, const char* redirect_uri);

/**
 * @brief Get Spotify controller handle
 *
 * @return spotify_controller_handle_t Spotify controller handle or NULL
 */
spotify_controller_handle_t esp_cast_get_spotify_controller(void);

/**
 * @brief Run Spotify periodic tasks
 *
 * Should be called from main loop to handle token refresh and other periodic tasks
 */
void esp_cast_spotify_run_tasks(void);

/**
 * @brief Cast Spotify track to discovered Chromecast device
 *
 * @param device_name Name of the Chromecast device to cast to
 * @param track_uri Spotify track URI to cast
 * @return true on success, false on failure
 */
bool esp_cast_spotify_to_chromecast(const char* device_name, const char* track_uri);

/**
 * @brief Get list of available Chromecast devices for Spotify casting (string array format)
 *
 * @param devices Buffer to store device information as strings
 * @param max_devices Maximum number of devices to return
 * @return Number of devices found
 */
int esp_cast_get_chromecast_devices_for_spotify_strings(char devices[][64], int max_devices);

/**
 * @brief Get list of available Chromecast devices for Spotify casting
 *
 * @param devices Buffer to store device information
 * @param max_devices Maximum number of devices to return
 * @return Number of devices found
 */
int esp_cast_get_chromecast_devices_for_spotify(chromecast_device_info_t* devices, int max_devices);

#ifdef __cplusplus
}
#endif