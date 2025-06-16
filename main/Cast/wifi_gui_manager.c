#include "wifi_gui_manager.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "wifi_gui_manager";

// GUI state
typedef struct {
    bool initialized;
    lv_obj_t *status_bar;
    lv_obj_t *scan_button;
    lv_obj_t *wifi_list_container;
    lv_obj_t *connection_modal;
    lv_obj_t *main_container;
} wifi_gui_state_t;

static wifi_gui_state_t g_gui_state = {0};

// Forward declarations
static void scan_button_cb(lv_event_t *e);
static void wifi_network_button_cb(lv_event_t *e);
static void password_input_cb(lv_event_t *e);
static void wifi_status_callback(const char *ssid, const char *ip, bool connected);
static void wifi_scan_callback(wifi_ap_record_t *aps, uint16_t ap_count);

esp_err_t wifi_gui_manager_init(const wifi_gui_config_t *config) {
    if (g_gui_state.initialized) {
        ESP_LOGW(TAG, "WiFi GUI Manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing WiFi GUI Manager");

    // Initialize WiFi Manager with callbacks
    wifi_manager_config_t wifi_config = {
        .status_callback = wifi_status_callback,
        .scan_callback = wifi_scan_callback,
        .auto_connect = true,
        .scan_timeout_ms = 10000
    };

    esp_err_t ret = wifi_manager_init(&wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi Manager: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create GUI elements if parent provided
    if (config && config->parent) {
        g_gui_state.main_container = wifi_gui_create_interface(config->parent);
    }

    g_gui_state.initialized = true;
    ESP_LOGI(TAG, "WiFi GUI Manager initialized successfully");
    return ESP_OK;
}

esp_err_t wifi_gui_manager_deinit(void) {
    if (!g_gui_state.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing WiFi GUI Manager");

    // Clean up GUI objects
    if (g_gui_state.main_container) {
        lv_obj_del(g_gui_state.main_container);
    }
    if (g_gui_state.connection_modal) {
        lv_obj_del(g_gui_state.connection_modal);
    }

    // Deinitialize WiFi Manager
    wifi_manager_deinit();

    // Clear state
    memset(&g_gui_state, 0, sizeof(g_gui_state));
    
    ESP_LOGI(TAG, "WiFi GUI Manager deinitialized");
    return ESP_OK;
}

lv_obj_t *wifi_gui_create_interface(lv_obj_t *parent) {
    if (!parent) {
        parent = lv_scr_act();
    }

    // Create main container
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    lv_obj_center(container);

    // Create button list
    lv_obj_t *btn_list = lv_list_create(container);
    lv_obj_center(btn_list);

    // Create WiFi scan button
    g_gui_state.scan_button = lv_list_add_btn(btn_list, LV_SYMBOL_WIFI, "Scan Wi-Fi");
    lv_obj_add_event_cb(g_gui_state.scan_button, scan_button_cb, LV_EVENT_CLICKED, NULL);

    // Create status bar
    g_gui_state.status_bar = lv_label_create(container);
    lv_obj_align(g_gui_state.status_bar, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_label_set_text(g_gui_state.status_bar, "Wi-Fi: Disconnected");

    g_gui_state.main_container = container;
    return container;
}

void wifi_gui_show_scan_results(wifi_ap_record_t *aps, uint16_t ap_count) {
    // Delete previous list if it exists
    if (g_gui_state.wifi_list_container) {
        lv_obj_del(g_gui_state.wifi_list_container);
        g_gui_state.wifi_list_container = NULL;
    }

    if (ap_count == 0) {
        ESP_LOGI(TAG, "No WiFi networks found");
        return;
    }

    // Create new list container
    g_gui_state.wifi_list_container = lv_list_create(g_gui_state.main_container);
    lv_obj_set_size(g_gui_state.wifi_list_container, lv_pct(90), LV_SIZE_CONTENT);
    lv_obj_center(g_gui_state.wifi_list_container);

    // Add networks to list
    for (int i = 0; i < ap_count; i++) {
        // Create button with SSID and signal strength
        char btn_text[64];
        snprintf(btn_text, sizeof(btn_text), "%s (%d dBm)", 
                 (char *)aps[i].ssid, aps[i].rssi);

        lv_obj_t *btn = lv_list_add_btn(g_gui_state.wifi_list_container, LV_SYMBOL_WIFI, btn_text);
        
        // Store AP info in button user data
        wifi_ap_record_t *ap_data = malloc(sizeof(wifi_ap_record_t));
        if (ap_data) {
            memcpy(ap_data, &aps[i], sizeof(wifi_ap_record_t));
            lv_obj_set_user_data(btn, ap_data);
            lv_obj_add_event_cb(btn, wifi_network_button_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    ESP_LOGI(TAG, "Displayed %d WiFi networks", ap_count);
}

void wifi_gui_update_status(const char *ssid, const char *ip_address, bool connected) {
    if (!g_gui_state.status_bar) {
        return;
    }

    char status_text[128];
    if (connected && ssid && ip_address) {
        snprintf(status_text, sizeof(status_text), "Wi-Fi: Connected to %s (%s)", ssid, ip_address);
    } else {
        snprintf(status_text, sizeof(status_text), "Wi-Fi: Disconnected");
    }

    lv_label_set_text(g_gui_state.status_bar, status_text);
    ESP_LOGI(TAG, "Updated WiFi status: %s", status_text);
}

void wifi_gui_show_connection_dialog(const char *ssid) {
    if (!ssid || g_gui_state.connection_modal) {
        return;
    }

    // Create modal background
    g_gui_state.connection_modal = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_gui_state.connection_modal, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(g_gui_state.connection_modal, LV_OPA_50, 0);
    lv_obj_center(g_gui_state.connection_modal);

    // Create dialog container
    lv_obj_t *dialog = lv_obj_create(g_gui_state.connection_modal);
    lv_obj_set_size(dialog, 300, 250);
    lv_obj_center(dialog);

    // Create title label
    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text_fmt(title, "Connect to %s", ssid);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Create password input
    lv_obj_t *password_input = lv_textarea_create(dialog);
    lv_textarea_set_one_line(password_input, true);
    lv_textarea_set_password_mode(password_input, true);
    lv_textarea_set_placeholder_text(password_input, "Enter Wi-Fi Password");
    lv_obj_set_width(password_input, lv_pct(90));
    lv_obj_align(password_input, LV_ALIGN_TOP_MID, 0, 50);

    // Store SSID in input user data
    char *ssid_copy = malloc(strlen(ssid) + 1);
    if (ssid_copy) {
        strcpy(ssid_copy, ssid);
        lv_obj_set_user_data(password_input, ssid_copy);
    }

    // Create keyboard
    lv_obj_t *keyboard = lv_keyboard_create(dialog);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(keyboard, password_input);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, -10);

    // Handle password submission
    lv_obj_add_event_cb(password_input, password_input_cb, LV_EVENT_READY, NULL);

    ESP_LOGI(TAG, "Showing connection dialog for SSID: %s", ssid);
}

void wifi_gui_hide_scan_results(void) {
    if (g_gui_state.wifi_list_container) {
        lv_obj_del(g_gui_state.wifi_list_container);
        g_gui_state.wifi_list_container = NULL;
    }
}

lv_obj_t *wifi_gui_get_status_bar(void) {
    return g_gui_state.status_bar;
}

void wifi_gui_set_status_bar_position(lv_align_t align, int32_t x_offset, int32_t y_offset) {
    if (g_gui_state.status_bar) {
        lv_obj_align(g_gui_state.status_bar, align, x_offset, y_offset);
    }
}

// Callback implementations
static void scan_button_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "WiFi scan button clicked");

    esp_err_t ret = wifi_manager_scan(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi scan: %s", esp_err_to_name(ret));
    }
}

static void wifi_network_button_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    wifi_ap_record_t *ap = (wifi_ap_record_t *)lv_obj_get_user_data(btn);

    if (ap) {
        ESP_LOGI(TAG, "Selected WiFi network: %s", (char *)ap->ssid);
        wifi_gui_show_connection_dialog((char *)ap->ssid);
    }
}

static void password_input_cb(lv_event_t *e) {
    lv_obj_t *input = lv_event_get_target(e);
    char *ssid = (char *)lv_obj_get_user_data(input);
    const char *password = lv_textarea_get_text(input);

    if (ssid && password) {
        ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);

        esp_err_t ret = wifi_manager_connect(ssid, password, true);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to connect to WiFi: %s", esp_err_to_name(ret));
        }

        // Clean up
        free(ssid);

        // Close modal
        if (g_gui_state.connection_modal) {
            lv_obj_del_async(g_gui_state.connection_modal);
            g_gui_state.connection_modal = NULL;
        }

        // Hide scan results
        wifi_gui_hide_scan_results();
    }
}

static void wifi_status_callback(const char *ssid, const char *ip, bool connected) {
    ESP_LOGI(TAG, "WiFi status changed: %s", connected ? "Connected" : "Disconnected");
    wifi_gui_update_status(ssid, ip, connected);
}

static void wifi_scan_callback(wifi_ap_record_t *aps, uint16_t ap_count) {
    ESP_LOGI(TAG, "WiFi scan completed, found %d networks", ap_count);
    wifi_gui_show_scan_results(aps, ap_count);
}
