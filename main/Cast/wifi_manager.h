#pragma once

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi Manager - Handles all WiFi-related functionality
 * 
 * This class manages WiFi initialization, scanning, connection, credential storage,
 * and provides GUI integration for WiFi management.
 */

// Maximum number of AP records to retrieve during scan
#define WIFI_MANAGER_MAX_AP_RECORDS 10

// WiFi credential storage keys
#define WIFI_CREDS_NAMESPACE "wifi_creds"
#define WIFI_CREDS_SSID_KEY "ssid"
#define WIFI_CREDS_PASS_KEY "pass"

// WiFi connection status callback
typedef void (*wifi_status_callback_t)(const char *ssid, const char *ip, bool connected);

// WiFi scan results callback
typedef void (*wifi_scan_callback_t)(wifi_ap_record_t *aps, uint16_t ap_count);

/**
 * @brief WiFi Manager configuration structure
 */
typedef struct {
    wifi_status_callback_t status_callback;    // Called when WiFi status changes
    wifi_scan_callback_t scan_callback;        // Called when scan completes
    bool auto_connect;                          // Auto-connect to saved credentials
    uint32_t scan_timeout_ms;                   // Scan timeout in milliseconds
} wifi_manager_config_t;

/**
 * @brief WiFi connection information
 */
typedef struct {
    char ssid[33];          // SSID (max 32 chars + null terminator)
    char ip_address[16];    // IP address as string
    bool connected;         // Connection status
    int8_t rssi;           // Signal strength
} wifi_connection_info_t;

/**
 * @brief Initialize WiFi Manager
 * 
 * @param config Configuration structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_manager_init(const wifi_manager_config_t *config);

/**
 * @brief Deinitialize WiFi Manager
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_manager_deinit(void);

/**
 * @brief Start WiFi scan
 * 
 * @param show_hidden Include hidden networks
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_manager_scan(bool show_hidden);

/**
 * @brief Connect to WiFi network
 * 
 * @param ssid Network SSID
 * @param password Network password
 * @param save_credentials Save credentials to NVS
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_manager_connect(const char *ssid, const char *password, bool save_credentials);

/**
 * @brief Disconnect from current WiFi network
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_manager_disconnect(void);

/**
 * @brief Get current WiFi connection information
 * 
 * @param info Pointer to store connection information
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_manager_get_connection_info(wifi_connection_info_t *info);

/**
 * @brief Load saved WiFi credentials from NVS
 * 
 * @param ssid Buffer to store SSID
 * @param ssid_len SSID buffer length
 * @param password Buffer to store password
 * @param pass_len Password buffer length
 * @return bool true if credentials loaded successfully
 */
bool wifi_manager_load_credentials(char *ssid, size_t ssid_len, char *password, size_t pass_len);

/**
 * @brief Save WiFi credentials to NVS
 * 
 * @param ssid Network SSID
 * @param password Network password
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password);

/**
 * @brief Clear saved WiFi credentials from NVS
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_manager_clear_credentials(void);

/**
 * @brief Check if WiFi is connected
 * 
 * @return bool true if connected
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Get WiFi signal strength (RSSI)
 * 
 * @return int8_t RSSI value in dBm
 */
int8_t wifi_manager_get_rssi(void);

/**
 * @brief Set WiFi status callback
 * 
 * @param callback Callback function
 */
void wifi_manager_set_status_callback(wifi_status_callback_t callback);

/**
 * @brief Set WiFi scan callback
 *
 * @param callback Callback function
 */
void wifi_manager_set_scan_callback(wifi_scan_callback_t callback);

/**
 * @brief Manually trigger auto-connect to saved credentials
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_manager_auto_connect(void);

/**
 * @brief Try connecting to default WiFi credentials from project configuration
 *
 * @return esp_err_t ESP_OK on success, ESP_ERR_NOT_FOUND if no default credentials configured
 */
esp_err_t wifi_manager_try_default_credentials(void);

#ifdef __cplusplus
}
#endif
