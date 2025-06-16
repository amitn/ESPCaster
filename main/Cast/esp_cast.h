#pragma once

#include "esp_wifi.h"
#include "lvgl.h"

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

#ifdef __cplusplus
}
#endif