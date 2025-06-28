#include "esp_event.h"
#include "esp_log.h"
#include "esp_cast.h"
#include "wifi_gui_manager.h"
#include "wifi_manager.h"
#include "chromecast_discovery_wrapper.h"
#include "chromecast_gui_manager.h"
#include "spotify_controller_wrapper.h"
#include "spotify_gui_manager.h"
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
static spotify_controller_handle_t spotify_handle = NULL;
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

    // Create Spotify tab
    lv_obj_t *spotify_tab = lv_tabview_add_tab(main_tabview, "Spotify");

    // Initialize Spotify GUI manager (if Spotify controller is available)
    if (spotify_handle) {
        spotify_gui_config_t spotify_config = {
            .parent = spotify_tab,
            .controller = spotify_handle,
            .show_status_bar = true,
            .show_auth_button = true,
            .show_search_bar = true
        };

        esp_err_t ret = spotify_gui_manager_init(&spotify_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize Spotify GUI Manager: %s", esp_err_to_name(ret));
        } else {
            // Create Spotify interface
            spotify_gui_create_interface(spotify_tab);
            ESP_LOGI(TAG, "Spotify GUI initialized successfully");
        }
    } else {
        // Create placeholder for Spotify tab
        lv_obj_t *placeholder = lv_label_create(spotify_tab);
        lv_label_set_text(placeholder, "Spotify not configured.\nPlease initialize with client credentials.");
        lv_obj_center(placeholder);
    }

    ESP_LOGI(TAG, "ESP Cast GUI initialized with WiFi, Chromecast, and Spotify tabs");
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

    // Run Spotify periodic tasks
    esp_cast_spotify_run_tasks();
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

bool esp_cast_spotify_init(const char* client_id, const char* client_secret, const char* redirect_uri) {
    if (!client_id) {
        ESP_LOGE(TAG, "Spotify client ID is required");
        return false;
    }

    ESP_LOGI(TAG, "Initializing Spotify controller");

    // Create Spotify controller
    spotify_handle = spotify_controller_create();
    if (!spotify_handle) {
        ESP_LOGE(TAG, "Failed to create Spotify controller");
        return false;
    }

    // Initialize controller
    if (!spotify_controller_initialize(spotify_handle, client_id, client_secret, redirect_uri)) {
        ESP_LOGE(TAG, "Failed to initialize Spotify controller");
        spotify_controller_destroy(spotify_handle);
        spotify_handle = NULL;
        return false;
    }

    ESP_LOGI(TAG, "Spotify controller initialized successfully");
    return true;
}

spotify_controller_handle_t esp_cast_get_spotify_controller(void) {
    return spotify_handle;
}

void esp_cast_spotify_run_tasks(void) {
    if (spotify_handle) {
        spotify_controller_run_periodic_tasks(spotify_handle);
    }
}

bool esp_cast_spotify_to_chromecast(const char* device_name, const char* track_uri) {
    if (!device_name || !track_uri || !spotify_handle || !discovery_handle) {
        ESP_LOGE(TAG, "Invalid parameters or components not initialized");
        return false;
    }

    ESP_LOGI(TAG, "Attempting to cast Spotify track to Chromecast device: %s", device_name);

    // Get the list of discovered Chromecast devices
    chromecast_device_info_t devices[10];
    size_t device_count = 0;

    // Note: In a real implementation, you would need to store the discovered devices
    // from the discovery callback. For now, we'll simulate this.

    // Find the device by name
    bool device_found = false;
    char target_ip[16] = {0};

    // This is a simplified approach - in practice, you'd maintain a list of discovered devices
    ESP_LOGI(TAG, "Looking for Chromecast device: %s", device_name);

    // For demonstration, we'll assume the device is found and use a placeholder IP
    // In a real implementation, you would:
    // 1. Query the discovery component for current devices
    // 2. Match by device name
    // 3. Get the IP address

    if (strstr(device_name, "Chromecast") != NULL) {
        device_found = true;
        strncpy(target_ip, "192.168.1.100", sizeof(target_ip) - 1); // Placeholder IP
        ESP_LOGI(TAG, "Found device %s at IP %s (simulated)", device_name, target_ip);
    }

    if (!device_found) {
        ESP_LOGE(TAG, "Chromecast device not found: %s", device_name);
        return false;
    }

    // Cast to the Chromecast device
    bool result = spotify_controller_cast_to_chromecast(spotify_handle, target_ip, track_uri);

    if (result) {
        ESP_LOGI(TAG, "Successfully initiated casting to %s", device_name);
    } else {
        ESP_LOGE(TAG, "Failed to cast to %s", device_name);
    }

    return result;
}

int esp_cast_get_chromecast_devices_for_spotify_strings(char devices[][64], int max_devices) {
    if (!devices || max_devices <= 0 || !discovery_handle) {
        ESP_LOGE(TAG, "Invalid parameters or discovery not initialized");
        return 0;
    }

    // In a real implementation, you would query the discovery component
    // for the current list of discovered devices

    // For demonstration, we'll return some mock devices
    int device_count = 0;

    if (max_devices > 0) {
        strncpy(devices[0], "Living Room Chromecast", 63);
        devices[0][63] = '\0';
        device_count++;
    }

    if (max_devices > 1) {
        strncpy(devices[1], "Bedroom Chromecast", 63);
        devices[1][63] = '\0';
        device_count++;
    }

    if (max_devices > 2) {
        strncpy(devices[2], "Kitchen Chromecast", 63);
        devices[2][63] = '\0';
        device_count++;
    }

    ESP_LOGI(TAG, "Returning %d Chromecast devices for Spotify casting", device_count);
    return device_count;
}

int esp_cast_get_chromecast_devices_for_spotify(chromecast_device_info_t* devices, int max_devices) {
    if (!devices || max_devices <= 0) {
        ESP_LOGE(TAG, "Invalid parameters for getting Chromecast devices");
        return 0;
    }

    ESP_LOGI(TAG, "Getting Chromecast devices for Spotify casting (device info format)");

    // For now, return simulated devices
    // In a real implementation, this would query the discovery component
    int device_count = 0;

    if (max_devices > 0) {
        strncpy(devices[0].name, "Living Room Chromecast", sizeof(devices[0].name) - 1);
        devices[0].name[sizeof(devices[0].name) - 1] = '\0';
        strncpy(devices[0].ip_address, "192.168.1.100", sizeof(devices[0].ip_address) - 1);
        devices[0].ip_address[sizeof(devices[0].ip_address) - 1] = '\0';
        devices[0].port = 8009;
        strncpy(devices[0].instance_name, "Chromecast-Living-Room", sizeof(devices[0].instance_name) - 1);
        devices[0].instance_name[sizeof(devices[0].instance_name) - 1] = '\0';
        strncpy(devices[0].model, "Chromecast", sizeof(devices[0].model) - 1);
        devices[0].model[sizeof(devices[0].model) - 1] = '\0';
        strncpy(devices[0].uuid, "12345678-1234-1234-1234-123456789abc", sizeof(devices[0].uuid) - 1);
        devices[0].uuid[sizeof(devices[0].uuid) - 1] = '\0';
        device_count++;
    }

    if (max_devices > 1) {
        strncpy(devices[1].name, "Bedroom Chromecast", sizeof(devices[1].name) - 1);
        devices[1].name[sizeof(devices[1].name) - 1] = '\0';
        strncpy(devices[1].ip_address, "192.168.1.101", sizeof(devices[1].ip_address) - 1);
        devices[1].ip_address[sizeof(devices[1].ip_address) - 1] = '\0';
        devices[1].port = 8009;
        strncpy(devices[1].instance_name, "Chromecast-Bedroom", sizeof(devices[1].instance_name) - 1);
        devices[1].instance_name[sizeof(devices[1].instance_name) - 1] = '\0';
        strncpy(devices[1].model, "Chromecast", sizeof(devices[1].model) - 1);
        devices[1].model[sizeof(devices[1].model) - 1] = '\0';
        strncpy(devices[1].uuid, "87654321-4321-4321-4321-cba987654321", sizeof(devices[1].uuid) - 1);
        devices[1].uuid[sizeof(devices[1].uuid) - 1] = '\0';
        device_count++;
    }

    ESP_LOGI(TAG, "Returning %d Chromecast devices for Spotify casting (device info format)", device_count);
    return device_count;
}
