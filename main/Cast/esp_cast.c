#include "esp_event.h"
#include "esp_log.h"
#include "esp_cast.h"
#include "wifi_gui_manager.h"
#include "wifi_manager.h"
#include "chromecast_discovery_wrapper.h"
#include "chromecast_gui_manager.h"
#include <string.h>

#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include "esp_system.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"

// Global variables
static const char *TAG = "esp_cast";
static chromecast_discovery_handle_t discovery_handle = NULL;
static lv_obj_t *main_tabview = NULL;

// Function prototypes
static void chromecast_discovery_callback(const chromecast_device_info_t* devices, size_t device_count);
static void chromecast_device_found_callback(const chromecast_device_info_t* device);
void esp_cast_wifi_init_sta(void) {
    ESP_LOGI(TAG, "Initializing WiFi via WiFi Manager");

    // Initialize WiFi GUI Manager (which also initializes WiFi Manager)
    wifi_gui_config_t config = {
        .parent = NULL,  // Will be set in GUI init
        .show_status_bar = true,
        .show_scan_button = true
    };

    esp_err_t ret = wifi_gui_manager_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi GUI Manager: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "WiFi GUI Manager initialized successfully");

    // Initialize ChromecastDiscovery
    discovery_handle = chromecast_discovery_create();
    if (!discovery_handle) {
        ESP_LOGE(TAG, "Failed to create ChromecastDiscovery instance");
        return;
    }

    // Set up callbacks
    chromecast_discovery_set_callback(discovery_handle, chromecast_discovery_callback);
    chromecast_discovery_set_device_found_callback(discovery_handle, chromecast_device_found_callback);

    // Configure discovery parameters
    chromecast_discovery_set_timeout(discovery_handle, 10000);  // 10 second timeout
    chromecast_discovery_set_max_results(discovery_handle, 20); // Max 20 devices

    // Initialize discovery
    if (!chromecast_discovery_initialize(discovery_handle)) {
        ESP_LOGE(TAG, "Failed to initialize ChromecastDiscovery");
        chromecast_discovery_destroy(discovery_handle);
        discovery_handle = NULL;
        return;
    }

    ESP_LOGI(TAG, "ChromecastDiscovery initialized successfully");
}

static void chromecast_discovery_callback_gui(const chromecast_device_info_t* devices, size_t device_count);

void esp_cast_gui_init(void) {
    ESP_LOGI(TAG, "Initializing ESP Cast GUI");

    // Create main tabview
    main_tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 45);

    // Create WiFi tab
    lv_obj_t *wifi_tab = lv_tabview_add_tab(main_tabview, "WiFi");
    wifi_gui_create_interface(wifi_tab);

    // Create Chromecast tab
    lv_obj_t *chromecast_tab = lv_tabview_add_tab(main_tabview, "Chromecast");

    // Initialize Chromecast GUI manager
    chromecast_gui_config_t chromecast_config = {
        .parent = chromecast_tab,
        .discovery = discovery_handle,
        .show_status_bar = true,
        .show_scan_button = true
    };

    esp_err_t ret = chromecast_gui_manager_init(&chromecast_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Chromecast GUI Manager: %s", esp_err_to_name(ret));
        return;
    }

    // Create Chromecast interface
    chromecast_gui_create_interface(chromecast_tab);

    // Set up discovery callback to update GUI
    chromecast_discovery_set_callback(discovery_handle, chromecast_discovery_callback_gui);

    ESP_LOGI(TAG, "ESP Cast GUI initialized with WiFi and Chromecast tabs");
}

static void chromecast_discovery_callback_gui(const chromecast_device_info_t* devices, size_t device_count) {
    ESP_LOGI(TAG, "Discovery completed, found %d Chromecast devices", device_count);

    // Update the GUI with discovered devices
    chromecast_gui_show_devices(devices, device_count);

    // Update status based on results
    if (device_count == 0) {
        chromecast_gui_update_status("No devices", "No Chromecast devices found", false);
    } else {
        char status_text[64];
        snprintf(status_text, sizeof(status_text), "Found %d device%s",
                 device_count, device_count == 1 ? "" : "s");
        chromecast_gui_update_status("Ready", status_text, false);
    }

    for (size_t i = 0; i < device_count; i++) {
        char device_str[256];
        if (chromecast_device_info_to_string(&devices[i], device_str, sizeof(device_str))) {
            ESP_LOGI(TAG, "  %s", device_str);
        }
    }
}

// WiFi GUI functions are now handled by wifi_gui_manager
// These functions are kept for backward compatibility but delegate to the new manager

void esp_cast_show_wifi_list(wifi_ap_record_t *aps, uint16_t ap_num) {
    // This function is now handled by wifi_gui_manager callbacks
    ESP_LOGI(TAG, "esp_cast_show_wifi_list called - delegating to wifi_gui_manager");
    wifi_gui_show_scan_results(aps, ap_num);
}

void gui_update_wifi_status(const char *ssid, const char *ip) {
    // This function is now handled by wifi_gui_manager callbacks
    ESP_LOGI(TAG, "gui_update_wifi_status called - delegating to wifi_gui_manager");
    wifi_gui_update_status(ssid, ip, (ssid != NULL && ip != NULL));
}

// ChromecastDiscovery callback implementations (legacy - now handled by GUI callback)
static void chromecast_discovery_callback(const chromecast_device_info_t* devices, size_t device_count) {
    ESP_LOGI(TAG, "Legacy discovery callback - found %d Chromecast devices", device_count);
    // This callback is now mainly for logging, GUI updates are handled by chromecast_discovery_callback_gui
}

static void chromecast_device_found_callback(const chromecast_device_info_t* device) {
    char device_str[256];
    if (chromecast_device_info_to_string(device, device_str, sizeof(device_str))) {
        ESP_LOGI(TAG, "Device found: %s", device_str);
    }
}

void esp_cast_loop(void) {
    // The new ChromecastDiscovery handles everything internally via callbacks
    // No polling needed in the main loop
}

void esp_cast_test_wifi_auto_connect(void) {
    ESP_LOGI(TAG, "Testing WiFi auto-connect functionality");

    // Check if WiFi manager is already connected
    if (wifi_manager_is_connected()) {
        ESP_LOGI(TAG, "WiFi is already connected");
        wifi_connection_info_t info;
        if (wifi_manager_get_connection_info(&info) == ESP_OK) {
            ESP_LOGI(TAG, "Current connection: SSID=%s, IP=%s, RSSI=%d dBm",
                     info.ssid, info.ip_address, info.rssi);
        }
        return;
    }

    // Try to auto-connect
    esp_err_t ret = wifi_manager_auto_connect();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Auto-connect initiated successfully");
    } else if (ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "No saved WiFi credentials found");
    } else {
        ESP_LOGE(TAG, "Auto-connect failed: %s", esp_err_to_name(ret));
    }
}

void esp_cast_test_default_wifi(void) {
    ESP_LOGI(TAG, "Testing default WiFi credentials functionality");

    // Try to connect using default credentials
    esp_err_t ret = wifi_manager_try_default_credentials();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Default WiFi connection initiated successfully");
    } else if (ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "No default WiFi credentials configured");
    } else {
        ESP_LOGE(TAG, "Default WiFi connection failed: %s", esp_err_to_name(ret));
    }
}
