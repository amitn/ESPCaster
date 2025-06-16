#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "wifi_manager";

// Internal state
typedef struct {
    bool initialized;
    bool connected;
    bool auto_connect_enabled;
    uint8_t reconnect_attempts;
    wifi_status_callback_t status_callback;
    wifi_scan_callback_t scan_callback;
    wifi_connection_info_t connection_info;
    esp_event_handler_instance_t wifi_handler_instance;
    esp_event_handler_instance_t ip_handler_instance;
} wifi_manager_state_t;

static wifi_manager_state_t g_wifi_state = {0};

// Forward declarations
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static esp_err_t wifi_manager_init_nvs(void);

esp_err_t wifi_manager_init(const wifi_manager_config_t *config) {
    if (g_wifi_state.initialized) {
        ESP_LOGW(TAG, "WiFi Manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing WiFi Manager");

    // Initialize NVS
    esp_err_t ret = wifi_manager_init_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &g_wifi_state.wifi_handler_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &g_wifi_state.ip_handler_instance));

    // Set callbacks if provided
    if (config) {
        g_wifi_state.status_callback = config->status_callback;
        g_wifi_state.scan_callback = config->scan_callback;
        g_wifi_state.auto_connect_enabled = config->auto_connect;
    }

    // Mark as initialized before attempting auto-connect
    g_wifi_state.initialized = true;
    ESP_LOGI(TAG, "WiFi Manager initialized successfully");

    // Auto-connect if enabled (must be done after marking as initialized)
    if (config && config->auto_connect) {
        // First try default credentials
        esp_err_t connect_ret = wifi_manager_try_default_credentials();
        if (connect_ret != ESP_OK) {
            // Fall back to saved credentials
            char ssid[33] = {0};
            char password[65] = {0};
            if (wifi_manager_load_credentials(ssid, sizeof(ssid), password, sizeof(password))) {
                ESP_LOGI(TAG, "Auto-connecting to saved network: %s", ssid);
                g_wifi_state.reconnect_attempts = 0;
                connect_ret = wifi_manager_connect(ssid, password, false);
                if (connect_ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to auto-connect to saved network: %s", esp_err_to_name(connect_ret));
                }
            } else {
                ESP_LOGI(TAG, "No saved WiFi credentials found, auto-connect disabled");
            }
        }
    }

    return ESP_OK;
}

esp_err_t wifi_manager_deinit(void) {
    if (!g_wifi_state.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing WiFi Manager");

    // Unregister event handlers
    if (g_wifi_state.wifi_handler_instance) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, g_wifi_state.wifi_handler_instance);
    }
    if (g_wifi_state.ip_handler_instance) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, g_wifi_state.ip_handler_instance);
    }

    // Stop and deinitialize WiFi
    esp_wifi_stop();
    esp_wifi_deinit();

    // Clear state
    memset(&g_wifi_state, 0, sizeof(g_wifi_state));
    
    ESP_LOGI(TAG, "WiFi Manager deinitialized");
    return ESP_OK;
}

esp_err_t wifi_manager_scan(bool show_hidden) {
    if (!g_wifi_state.initialized) {
        ESP_LOGE(TAG, "WiFi Manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = show_hidden
    };

    ESP_LOGI(TAG, "Starting WiFi scan");
    return esp_wifi_scan_start(&scan_config, false);
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password, bool save_credentials) {
    if (!g_wifi_state.initialized) {
        ESP_LOGE(TAG, "WiFi Manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!ssid) {
        ESP_LOGE(TAG, "SSID cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    ESP_LOGI(TAG, "Connecting to WiFi network: %s", ssid);

    // Disconnect first if already connected
    esp_wifi_disconnect();
    
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    // Save credentials if requested
    if (save_credentials) {
        wifi_manager_save_credentials(ssid, password);
    }

    return ESP_OK;
}

esp_err_t wifi_manager_disconnect(void) {
    if (!g_wifi_state.initialized) {
        ESP_LOGE(TAG, "WiFi Manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Disconnecting from WiFi");
    return esp_wifi_disconnect();
}

esp_err_t wifi_manager_get_connection_info(wifi_connection_info_t *info) {
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }

    *info = g_wifi_state.connection_info;
    return ESP_OK;
}

bool wifi_manager_load_credentials(char *ssid, size_t ssid_len, char *password, size_t pass_len) {
    if (!ssid || !password) {
        ESP_LOGE(TAG, "Invalid parameters for loading credentials");
        return false;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WIFI_CREDS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No saved WiFi credentials found (NVS namespace not found)");
        return false;
    }

    size_t required_ssid_len = ssid_len;
    err = nvs_get_str(nvs, WIFI_CREDS_SSID_KEY, ssid, &required_ssid_len);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No saved SSID found in NVS");
        nvs_close(nvs);
        return false;
    }

    size_t required_pass_len = pass_len;
    err = nvs_get_str(nvs, WIFI_CREDS_PASS_KEY, password, &required_pass_len);
    nvs_close(nvs);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Successfully loaded WiFi credentials for SSID: %s", ssid);
        return true;
    } else {
        ESP_LOGW(TAG, "Failed to load password for SSID: %s, error: %s", ssid, esp_err_to_name(err));
        return false;
    }
}

esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password) {
    if (!ssid) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WIFI_CREDS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing credentials");
        return err;
    }

    err = nvs_set_str(nvs, WIFI_CREDS_SSID_KEY, ssid);
    if (err == ESP_OK && password) {
        err = nvs_set_str(nvs, WIFI_CREDS_PASS_KEY, password);
    }
    
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    
    nvs_close(nvs);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Saved WiFi credentials for SSID: %s", ssid);
    } else {
        ESP_LOGE(TAG, "Failed to save WiFi credentials: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t wifi_manager_clear_credentials(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WIFI_CREDS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    nvs_erase_key(nvs, WIFI_CREDS_SSID_KEY);
    nvs_erase_key(nvs, WIFI_CREDS_PASS_KEY);
    err = nvs_commit(nvs);
    nvs_close(nvs);

    ESP_LOGI(TAG, "Cleared WiFi credentials");
    return err;
}

bool wifi_manager_is_connected(void) {
    return g_wifi_state.connected;
}

int8_t wifi_manager_get_rssi(void) {
    return g_wifi_state.connection_info.rssi;
}

void wifi_manager_set_status_callback(wifi_status_callback_t callback) {
    g_wifi_state.status_callback = callback;
}

void wifi_manager_set_scan_callback(wifi_scan_callback_t callback) {
    g_wifi_state.scan_callback = callback;
}

esp_err_t wifi_manager_try_default_credentials(void) {
    if (!g_wifi_state.initialized) {
        ESP_LOGE(TAG, "WiFi Manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

#ifdef CONFIG_DEFAULT_WIFI_ENABLED
    const char *default_ssid = CONFIG_DEFAULT_WIFI_SSID;
    const char *default_password = CONFIG_DEFAULT_WIFI_PASSWORD;

    if (strlen(default_ssid) == 0) {
        ESP_LOGI(TAG, "Default WiFi SSID not configured");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Trying to connect to default WiFi network: %s", default_ssid);
    g_wifi_state.reconnect_attempts = 0;
    return wifi_manager_connect(default_ssid, default_password, false);
#else
    ESP_LOGI(TAG, "Default WiFi credentials not enabled in configuration");
    return ESP_ERR_NOT_FOUND;
#endif
}

esp_err_t wifi_manager_auto_connect(void) {
    if (!g_wifi_state.initialized) {
        ESP_LOGE(TAG, "WiFi Manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // First try default credentials
    esp_err_t ret = wifi_manager_try_default_credentials();
    if (ret == ESP_OK) {
        return ESP_OK;
    }

    // Fall back to saved credentials
    char ssid[33] = {0};
    char password[65] = {0};

    if (!wifi_manager_load_credentials(ssid, sizeof(ssid), password, sizeof(password))) {
        ESP_LOGW(TAG, "No saved WiFi credentials found for auto-connect");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Manually triggering auto-connect to saved network: %s", ssid);
    g_wifi_state.reconnect_attempts = 0;
    return wifi_manager_connect(ssid, password, false);
}

// Internal helper functions
static esp_err_t wifi_manager_init_nvs(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "WiFi event: base=%s, id=%d", event_base, event_id);

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_SCAN_DONE: {
                uint16_t ap_count = 0;
                esp_wifi_scan_get_ap_num(&ap_count);

                if (ap_count > 0) {
                    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
                    if (ap_records) {
                        uint16_t actual_count = ap_count;
                        if (esp_wifi_scan_get_ap_records(&actual_count, ap_records) == ESP_OK) {
                            ESP_LOGI(TAG, "WiFi scan completed, found %d networks", actual_count);
                            if (g_wifi_state.scan_callback) {
                                g_wifi_state.scan_callback(ap_records, actual_count);
                            }
                        }
                        free(ap_records);
                    }
                } else {
                    ESP_LOGI(TAG, "WiFi scan completed, no networks found");
                    if (g_wifi_state.scan_callback) {
                        g_wifi_state.scan_callback(NULL, 0);
                    }
                }
                break;
            }

            case WIFI_EVENT_STA_CONNECTED: {
                wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
                ESP_LOGI(TAG, "Connected to WiFi network: %s", event->ssid);

                // Update connection info
                strncpy(g_wifi_state.connection_info.ssid, (char *)event->ssid, sizeof(g_wifi_state.connection_info.ssid) - 1);
                g_wifi_state.connection_info.connected = true;
                break;
            }

            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
                ESP_LOGI(TAG, "Disconnected from WiFi network, reason: %d", event->reason);

                // Update connection info
                memset(&g_wifi_state.connection_info, 0, sizeof(g_wifi_state.connection_info));
                g_wifi_state.connected = false;

                // Auto-reconnect if enabled and we have credentials
                if (g_wifi_state.auto_connect_enabled && g_wifi_state.reconnect_attempts < 5) {
                    g_wifi_state.reconnect_attempts++;
                    ESP_LOGI(TAG, "Auto-reconnecting (attempt %d/5)", g_wifi_state.reconnect_attempts);

                    // Wait a bit before reconnecting
                    vTaskDelay(pdMS_TO_TICKS(2000));

                    // Try default credentials first, then saved credentials
                    esp_err_t reconnect_ret = wifi_manager_try_default_credentials();
                    if (reconnect_ret != ESP_OK) {
                        char ssid[33] = {0};
                        char password[65] = {0};
                        if (wifi_manager_load_credentials(ssid, sizeof(ssid), password, sizeof(password))) {
                            ESP_LOGI(TAG, "Auto-reconnecting to saved network: %s", ssid);
                            esp_wifi_connect();
                        }
                    }
                } else if (g_wifi_state.reconnect_attempts >= 5) {
                    ESP_LOGW(TAG, "Max reconnection attempts reached, giving up auto-reconnect");
                }

                // Notify callback
                if (g_wifi_state.status_callback) {
                    g_wifi_state.status_callback(NULL, NULL, false);
                }
                break;
            }

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        // Convert IP to string
        esp_ip4addr_ntoa(&event->ip_info.ip, g_wifi_state.connection_info.ip_address,
                         sizeof(g_wifi_state.connection_info.ip_address));

        g_wifi_state.connected = true;
        g_wifi_state.connection_info.connected = true;
        g_wifi_state.reconnect_attempts = 0;  // Reset reconnect attempts on successful connection

        // Get RSSI
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            g_wifi_state.connection_info.rssi = ap_info.rssi;
        }

        ESP_LOGI(TAG, "Got IP address: %s", g_wifi_state.connection_info.ip_address);

        // Notify callback
        if (g_wifi_state.status_callback) {
            g_wifi_state.status_callback(g_wifi_state.connection_info.ssid,
                                       g_wifi_state.connection_info.ip_address, true);
        }
    }
}
