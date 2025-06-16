#pragma once

#include "chromecast_discovery_wrapper.h"
#include "chromecast_controller_wrapper.h"
#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Chromecast GUI Manager - Handles LVGL interface for Chromecast management
 * 
 * This component provides a graphical user interface for Chromecast management
 * using LVGL, including device discovery, device selection, and volume control.
 */

/**
 * @brief Chromecast GUI Manager configuration
 */
typedef struct {
    lv_obj_t *parent;                           // Parent object for Chromecast GUI elements
    chromecast_discovery_handle_t discovery;   // Discovery handle
    bool show_status_bar;                       // Show Chromecast status bar
    bool show_scan_button;                      // Show device scan button
} chromecast_gui_config_t;

/**
 * @brief Initialize Chromecast GUI Manager
 * 
 * @param config GUI configuration
 * @return esp_err_t ESP_OK on success
 */
esp_err_t chromecast_gui_manager_init(const chromecast_gui_config_t *config);

/**
 * @brief Deinitialize Chromecast GUI Manager
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t chromecast_gui_manager_deinit(void);

/**
 * @brief Create Chromecast management interface
 * 
 * Creates the main Chromecast management interface with scan button and status display
 * 
 * @param parent Parent LVGL object
 * @return lv_obj_t* Created container object
 */
lv_obj_t *chromecast_gui_create_interface(lv_obj_t *parent);

/**
 * @brief Show discovered Chromecast devices
 * 
 * @param devices Array of discovered devices
 * @param device_count Number of devices
 */
void chromecast_gui_show_devices(const chromecast_device_info_t *devices, size_t device_count);

/**
 * @brief Hide device list
 */
void chromecast_gui_hide_devices(void);

/**
 * @brief Update Chromecast connection status
 * 
 * @param device_name Name of connected device (NULL if disconnected)
 * @param ip_address IP address of connected device (NULL if disconnected)
 * @param connected Connection status
 */
void chromecast_gui_update_status(const char *device_name, const char *ip_address, bool connected);

/**
 * @brief Show volume control interface for selected device
 * 
 * @param device_info Device information
 */
void chromecast_gui_show_volume_control(const chromecast_device_info_t *device_info);

/**
 * @brief Hide volume control interface
 */
void chromecast_gui_hide_volume_control(void);

/**
 * @brief Update volume display
 * 
 * @param volume_info Current volume information
 */
void chromecast_gui_update_volume(const chromecast_volume_info_t *volume_info);

/**
 * @brief Show connection dialog for device
 * 
 * @param device_name Device name to connect to
 */
void chromecast_gui_show_connection_dialog(const char *device_name);

/**
 * @brief Get status bar object
 * 
 * @return lv_obj_t* Status bar object or NULL
 */
lv_obj_t *chromecast_gui_get_status_bar(void);

/**
 * @brief Set status bar position
 * 
 * @param align Alignment
 * @param x_offset X offset
 * @param y_offset Y offset
 */
void chromecast_gui_set_status_bar_position(lv_align_t align, int32_t x_offset, int32_t y_offset);

/**
 * @brief Set discovery handle for the GUI manager
 * 
 * @param discovery Discovery handle
 */
void chromecast_gui_set_discovery_handle(chromecast_discovery_handle_t discovery);

/**
 * @brief Get current controller handle
 * 
 * @return chromecast_controller_handle_t Current controller handle or NULL
 */
chromecast_controller_handle_t chromecast_gui_get_controller_handle(void);

#ifdef __cplusplus
}
#endif
