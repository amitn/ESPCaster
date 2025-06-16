#pragma once

#include "wifi_manager.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi GUI Manager - Handles LVGL interface for WiFi management
 * 
 * This component provides a graphical user interface for WiFi management
 * using LVGL, including WiFi scanning, network selection, and status display.
 */

/**
 * @brief WiFi GUI Manager configuration
 */
typedef struct {
    lv_obj_t *parent;           // Parent object for WiFi GUI elements
    bool show_status_bar;       // Show WiFi status bar
    bool show_scan_button;      // Show WiFi scan button
} wifi_gui_config_t;

/**
 * @brief Initialize WiFi GUI Manager
 * 
 * @param config GUI configuration
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_gui_manager_init(const wifi_gui_config_t *config);

/**
 * @brief Deinitialize WiFi GUI Manager
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_gui_manager_deinit(void);

/**
 * @brief Create WiFi management interface
 * 
 * Creates the main WiFi management interface with scan button and status display
 * 
 * @param parent Parent LVGL object
 * @return lv_obj_t* Created container object
 */
lv_obj_t *wifi_gui_create_interface(lv_obj_t *parent);

/**
 * @brief Show WiFi scan results
 * 
 * @param aps Array of WiFi access points
 * @param ap_count Number of access points
 */
void wifi_gui_show_scan_results(wifi_ap_record_t *aps, uint16_t ap_count);

/**
 * @brief Update WiFi status display
 * 
 * @param ssid Connected network SSID (NULL if disconnected)
 * @param ip_address IP address (NULL if disconnected)
 * @param connected Connection status
 */
void wifi_gui_update_status(const char *ssid, const char *ip_address, bool connected);

/**
 * @brief Show WiFi connection dialog
 * 
 * @param ssid Network SSID to connect to
 */
void wifi_gui_show_connection_dialog(const char *ssid);

/**
 * @brief Hide WiFi scan results
 */
void wifi_gui_hide_scan_results(void);

/**
 * @brief Get WiFi status bar object
 * 
 * @return lv_obj_t* Status bar label object
 */
lv_obj_t *wifi_gui_get_status_bar(void);

/**
 * @brief Set WiFi status bar position
 * 
 * @param align Alignment
 * @param x_offset X offset
 * @param y_offset Y offset
 */
void wifi_gui_set_status_bar_position(lv_align_t align, int32_t x_offset, int32_t y_offset);

#ifdef __cplusplus
}
#endif
