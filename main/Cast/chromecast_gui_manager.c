#include "chromecast_gui_manager.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "chromecast_gui_manager";

// GUI state
typedef struct {
    bool initialized;
    lv_obj_t *status_bar;
    lv_obj_t *scan_button;
    lv_obj_t *device_list_container;
    lv_obj_t *volume_control_container;
    lv_obj_t *volume_slider;
    lv_obj_t *mute_button;
    lv_obj_t *volume_label;
    lv_obj_t *connection_modal;
    lv_obj_t *main_container;
    chromecast_discovery_handle_t discovery_handle;
    chromecast_controller_handle_t controller_handle;
    chromecast_device_info_t selected_device;
    bool device_selected;
} chromecast_gui_state_t;

static chromecast_gui_state_t g_gui_state = {0};

// Forward declarations
static void scan_button_cb(lv_event_t *e);
static void device_button_cb(lv_event_t *e);
static void volume_slider_cb(lv_event_t *e);
static void mute_button_cb(lv_event_t *e);
static void back_button_cb(lv_event_t *e);
static void connect_button_cb(lv_event_t *e);
static void cancel_button_cb(lv_event_t *e);
static void chromecast_state_callback(chromecast_connection_state_t state);
static void chromecast_volume_callback(const chromecast_volume_info_t* volume);

esp_err_t chromecast_gui_manager_init(const chromecast_gui_config_t *config) {
    if (g_gui_state.initialized) {
        ESP_LOGW(TAG, "Chromecast GUI Manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing Chromecast GUI Manager");

    // Store discovery handle
    if (config && config->discovery) {
        g_gui_state.discovery_handle = config->discovery;
    }

    // Create controller instance
    g_gui_state.controller_handle = chromecast_controller_create();
    if (!g_gui_state.controller_handle) {
        ESP_LOGE(TAG, "Failed to create ChromecastController");
        return ESP_FAIL;
    }

    // Initialize controller
    if (!chromecast_controller_initialize(g_gui_state.controller_handle)) {
        ESP_LOGE(TAG, "Failed to initialize ChromecastController");
        chromecast_controller_destroy(g_gui_state.controller_handle);
        g_gui_state.controller_handle = NULL;
        return ESP_FAIL;
    }

    // Set up controller callbacks
    chromecast_controller_set_state_callback(g_gui_state.controller_handle, chromecast_state_callback);
    chromecast_controller_set_volume_callback(g_gui_state.controller_handle, chromecast_volume_callback);

    // Create GUI elements if parent provided
    if (config && config->parent) {
        g_gui_state.main_container = chromecast_gui_create_interface(config->parent);
    }

    g_gui_state.initialized = true;
    ESP_LOGI(TAG, "Chromecast GUI Manager initialized successfully");
    return ESP_OK;
}

esp_err_t chromecast_gui_manager_deinit(void) {
    if (!g_gui_state.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing Chromecast GUI Manager");

    // Clean up controller
    if (g_gui_state.controller_handle) {
        chromecast_controller_destroy(g_gui_state.controller_handle);
        g_gui_state.controller_handle = NULL;
    }

    // Clean up GUI elements
    chromecast_gui_hide_devices();
    chromecast_gui_hide_volume_control();

    if (g_gui_state.connection_modal) {
        lv_obj_del(g_gui_state.connection_modal);
        g_gui_state.connection_modal = NULL;
    }

    memset(&g_gui_state, 0, sizeof(g_gui_state));
    ESP_LOGI(TAG, "Chromecast GUI Manager deinitialized");
    return ESP_OK;
}

lv_obj_t *chromecast_gui_create_interface(lv_obj_t *parent) {
    if (!parent) {
        ESP_LOGE(TAG, "Parent object is NULL");
        return NULL;
    }

    ESP_LOGI(TAG, "Creating Chromecast GUI interface");

    // Create main container
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    lv_obj_center(container);

    // Create button list
    lv_obj_t *btn_list = lv_list_create(container);
    lv_obj_center(btn_list);

    // Create Chromecast scan button
    g_gui_state.scan_button = lv_list_add_btn(btn_list, LV_SYMBOL_REFRESH, "Scan Chromecast");
    lv_obj_add_event_cb(g_gui_state.scan_button, scan_button_cb, LV_EVENT_CLICKED, NULL);

    // Create status bar
    g_gui_state.status_bar = lv_label_create(container);
    lv_obj_align(g_gui_state.status_bar, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_label_set_text(g_gui_state.status_bar, "Chromecast: Disconnected");

    g_gui_state.main_container = container;
    return container;
}

void chromecast_gui_show_devices(const chromecast_device_info_t *devices, size_t device_count) {
    if (!devices || device_count == 0 || !g_gui_state.main_container) {
        ESP_LOGW(TAG, "No devices to display or main container not available");
        return;
    }

    ESP_LOGI(TAG, "Displaying %d Chromecast devices", device_count);

    // Remove existing device list
    chromecast_gui_hide_devices();

    // Create new list container
    g_gui_state.device_list_container = lv_list_create(g_gui_state.main_container);
    lv_obj_set_size(g_gui_state.device_list_container, lv_pct(90), LV_SIZE_CONTENT);
    lv_obj_center(g_gui_state.device_list_container);

    // Add devices to list
    for (size_t i = 0; i < device_count; i++) {
        // Create button with device name and IP
        char btn_text[128];
        snprintf(btn_text, sizeof(btn_text), "%s (%s)", 
                 devices[i].name, devices[i].ip_address);

        lv_obj_t *btn = lv_list_add_btn(g_gui_state.device_list_container, LV_SYMBOL_AUDIO, btn_text);
        
        // Store device info in button user data
        chromecast_device_info_t *device_data = malloc(sizeof(chromecast_device_info_t));
        if (device_data) {
            memcpy(device_data, &devices[i], sizeof(chromecast_device_info_t));
            lv_obj_set_user_data(btn, device_data);
            lv_obj_add_event_cb(btn, device_button_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    ESP_LOGI(TAG, "Displayed %d Chromecast devices", device_count);
}

void chromecast_gui_hide_devices(void) {
    if (g_gui_state.device_list_container) {
        // Free user data from buttons
        uint32_t child_count = lv_obj_get_child_cnt(g_gui_state.device_list_container);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t *child = lv_obj_get_child(g_gui_state.device_list_container, i);
            void *user_data = lv_obj_get_user_data(child);
            if (user_data) {
                free(user_data);
            }
        }
        
        lv_obj_del(g_gui_state.device_list_container);
        g_gui_state.device_list_container = NULL;
    }
}

void chromecast_gui_update_status(const char *device_name, const char *ip_address, bool connected) {
    if (!g_gui_state.status_bar) {
        return;
    }

    char status_text[128];
    if (connected && device_name && ip_address) {
        snprintf(status_text, sizeof(status_text), "Chromecast: Connected to %s (%s)", device_name, ip_address);
    } else {
        snprintf(status_text, sizeof(status_text), "Chromecast: Disconnected");
    }

    lv_label_set_text(g_gui_state.status_bar, status_text);
    ESP_LOGI(TAG, "Updated Chromecast status: %s", status_text);
}

void chromecast_gui_show_volume_control(const chromecast_device_info_t *device_info) {
    if (!device_info || !g_gui_state.main_container) {
        return;
    }

    ESP_LOGI(TAG, "Showing volume control for device: %s", device_info->name);

    // Hide device list
    chromecast_gui_hide_devices();

    // Create volume control container
    g_gui_state.volume_control_container = lv_obj_create(g_gui_state.main_container);
    lv_obj_set_size(g_gui_state.volume_control_container, lv_pct(90), lv_pct(80));
    lv_obj_center(g_gui_state.volume_control_container);

    // Device name label
    lv_obj_t *device_label = lv_label_create(g_gui_state.volume_control_container);
    lv_label_set_text_fmt(device_label, "Controlling: %s", device_info->name);
    lv_obj_align(device_label, LV_ALIGN_TOP_MID, 0, 10);

    // Volume label
    g_gui_state.volume_label = lv_label_create(g_gui_state.volume_control_container);
    lv_label_set_text(g_gui_state.volume_label, "Volume: 50%");
    lv_obj_align(g_gui_state.volume_label, LV_ALIGN_TOP_MID, 0, 40);

    // Volume slider
    g_gui_state.volume_slider = lv_slider_create(g_gui_state.volume_control_container);
    lv_obj_set_size(g_gui_state.volume_slider, 200, 20);
    lv_obj_align(g_gui_state.volume_slider, LV_ALIGN_CENTER, 0, -20);
    lv_slider_set_range(g_gui_state.volume_slider, 0, 100);
    lv_slider_set_value(g_gui_state.volume_slider, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(g_gui_state.volume_slider, volume_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Mute button
    g_gui_state.mute_button = lv_btn_create(g_gui_state.volume_control_container);
    lv_obj_set_size(g_gui_state.mute_button, 100, 40);
    lv_obj_align(g_gui_state.mute_button, LV_ALIGN_CENTER, 0, 30);
    lv_obj_add_event_cb(g_gui_state.mute_button, mute_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *mute_label = lv_label_create(g_gui_state.mute_button);
    lv_label_set_text(mute_label, "Mute");
    lv_obj_center(mute_label);

    // Back button
    lv_obj_t *back_button = lv_btn_create(g_gui_state.volume_control_container);
    lv_obj_set_size(back_button, 80, 30);
    lv_obj_align(back_button, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_add_event_cb(back_button, back_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_button);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);

    // Store selected device
    memcpy(&g_gui_state.selected_device, device_info, sizeof(chromecast_device_info_t));
    g_gui_state.device_selected = true;

    // Get initial status
    if (g_gui_state.controller_handle) {
        chromecast_controller_get_status(g_gui_state.controller_handle);
    }
}

void chromecast_gui_hide_volume_control(void) {
    if (g_gui_state.volume_control_container) {
        lv_obj_del(g_gui_state.volume_control_container);
        g_gui_state.volume_control_container = NULL;
        g_gui_state.volume_slider = NULL;
        g_gui_state.mute_button = NULL;
        g_gui_state.volume_label = NULL;
    }
    g_gui_state.device_selected = false;
}

void chromecast_gui_update_volume(const chromecast_volume_info_t *volume_info) {
    if (!volume_info) return;

    if (g_gui_state.volume_slider) {
        int volume_percent = (int)(volume_info->level * 100);
        lv_slider_set_value(g_gui_state.volume_slider, volume_percent, LV_ANIM_ON);
    }

    if (g_gui_state.volume_label) {
        char volume_text[32];
        snprintf(volume_text, sizeof(volume_text), "Volume: %d%% %s",
                 (int)(volume_info->level * 100), volume_info->muted ? "(Muted)" : "");
        lv_label_set_text(g_gui_state.volume_label, volume_text);
    }

    if (g_gui_state.mute_button) {
        lv_obj_t *mute_label = lv_obj_get_child(g_gui_state.mute_button, 0);
        if (mute_label) {
            lv_label_set_text(mute_label, volume_info->muted ? "Unmute" : "Mute");
        }
    }

    ESP_LOGI(TAG, "Updated volume display: %.2f%% %s",
             volume_info->level * 100, volume_info->muted ? "(Muted)" : "");
}

void chromecast_gui_show_connection_dialog(const char *device_name) {
    if (!device_name || g_gui_state.connection_modal) {
        return;
    }

    ESP_LOGI(TAG, "Showing connection dialog for device: %s", device_name);

    // Create modal dialog
    g_gui_state.connection_modal = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_gui_state.connection_modal, 250, 150);
    lv_obj_center(g_gui_state.connection_modal);

    // Title
    lv_obj_t *title = lv_label_create(g_gui_state.connection_modal);
    lv_label_set_text_fmt(title, "Connect to %s?", device_name);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Connect button
    lv_obj_t *connect_btn = lv_btn_create(g_gui_state.connection_modal);
    lv_obj_set_size(connect_btn, 80, 30);
    lv_obj_align(connect_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(connect_btn, connect_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_center(connect_label);

    // Cancel button
    lv_obj_t *cancel_btn = lv_btn_create(g_gui_state.connection_modal);
    lv_obj_set_size(cancel_btn, 80, 30);
    lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_add_event_cb(cancel_btn, cancel_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);
}

lv_obj_t *chromecast_gui_get_status_bar(void) {
    return g_gui_state.status_bar;
}

void chromecast_gui_set_status_bar_position(lv_align_t align, int32_t x_offset, int32_t y_offset) {
    if (g_gui_state.status_bar) {
        lv_obj_align(g_gui_state.status_bar, align, x_offset, y_offset);
    }
}

void chromecast_gui_set_discovery_handle(chromecast_discovery_handle_t discovery) {
    g_gui_state.discovery_handle = discovery;
}

chromecast_controller_handle_t chromecast_gui_get_controller_handle(void) {
    return g_gui_state.controller_handle;
}

// Callback implementations
static void scan_button_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Chromecast scan button clicked");

    if (!g_gui_state.discovery_handle) {
        ESP_LOGE(TAG, "ChromecastDiscovery not initialized");
        chromecast_gui_update_status("Error", "Discovery not initialized", false);
        return;
    }

    // Update status to show scanning
    chromecast_gui_update_status("Scanning", "Looking for Chromecast devices...", false);

    // Start asynchronous discovery
    if (!chromecast_discovery_discover_async(g_gui_state.discovery_handle)) {
        ESP_LOGE(TAG, "Failed to start Chromecast discovery");
        chromecast_gui_update_status("Error", "Discovery failed - Check WiFi connection", false);
    }
}

static void device_button_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    chromecast_device_info_t *device = (chromecast_device_info_t *)lv_obj_get_user_data(btn);

    if (device) {
        ESP_LOGI(TAG, "Selected Chromecast device: %s", device->name);

        // Store selected device
        memcpy(&g_gui_state.selected_device, device, sizeof(chromecast_device_info_t));
        g_gui_state.device_selected = true;

        chromecast_gui_show_connection_dialog(device->name);
    }
}

static void volume_slider_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    float volume_level = value / 100.0f;

    ESP_LOGI(TAG, "Volume slider changed to: %d%%", value);

    if (g_gui_state.controller_handle && g_gui_state.device_selected) {
        // Get current mute state from the button label
        bool is_muted = false;
        if (g_gui_state.mute_button) {
            lv_obj_t *mute_label = lv_obj_get_child(g_gui_state.mute_button, 0);
            if (mute_label) {
                const char *label_text = lv_label_get_text(mute_label);
                is_muted = (strcmp(label_text, "Unmute") == 0);
            }
        }

        chromecast_controller_set_volume(g_gui_state.controller_handle, volume_level, is_muted);
    }
}

static void mute_button_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Mute button clicked");

    if (g_gui_state.controller_handle && g_gui_state.device_selected && g_gui_state.volume_slider) {
        // Get current volume level
        int32_t volume_value = lv_slider_get_value(g_gui_state.volume_slider);
        float volume_level = volume_value / 100.0f;

        // Toggle mute state
        lv_obj_t *mute_label = lv_obj_get_child(g_gui_state.mute_button, 0);
        bool is_currently_muted = false;
        if (mute_label) {
            const char *label_text = lv_label_get_text(mute_label);
            is_currently_muted = (strcmp(label_text, "Unmute") == 0);
        }

        chromecast_controller_set_volume(g_gui_state.controller_handle, volume_level, !is_currently_muted);
    }
}

static void back_button_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Back button clicked");

    // Disconnect from current device
    if (g_gui_state.controller_handle) {
        chromecast_controller_disconnect(g_gui_state.controller_handle);
    }

    // Hide volume control and show device list
    chromecast_gui_hide_volume_control();

    // Trigger a new scan to refresh the device list
    if (g_gui_state.discovery_handle) {
        chromecast_discovery_discover_async(g_gui_state.discovery_handle);
    }
}

static void connect_button_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Connect button clicked");

    if (g_gui_state.device_selected && g_gui_state.controller_handle) {
        // Connect to the selected device
        if (chromecast_controller_connect(g_gui_state.controller_handle, g_gui_state.selected_device.ip_address)) {
            ESP_LOGI(TAG, "Connection initiated to %s", g_gui_state.selected_device.name);

            // Show volume control interface
            chromecast_gui_show_volume_control(&g_gui_state.selected_device);

            // Start heartbeat
            chromecast_controller_start_heartbeat(g_gui_state.controller_handle);
        } else {
            ESP_LOGE(TAG, "Failed to connect to %s", g_gui_state.selected_device.name);
        }
    }

    // Close connection dialog
    if (g_gui_state.connection_modal) {
        lv_obj_del(g_gui_state.connection_modal);
        g_gui_state.connection_modal = NULL;
    }
}

static void chromecast_state_callback(chromecast_connection_state_t state) {
    const char *state_str = "Unknown";
    switch (state) {
        case CHROMECAST_DISCONNECTED:
            state_str = "Disconnected";
            chromecast_gui_update_status(NULL, NULL, false);
            break;
        case CHROMECAST_CONNECTING:
            state_str = "Connecting";
            break;
        case CHROMECAST_CONNECTED:
            state_str = "Connected";
            if (g_gui_state.device_selected) {
                chromecast_gui_update_status(g_gui_state.selected_device.name,
                                           g_gui_state.selected_device.ip_address, true);
            }
            break;
        case CHROMECAST_ERROR_STATE:
            state_str = "Error";
            chromecast_gui_update_status(NULL, NULL, false);
            break;
    }

    ESP_LOGI(TAG, "Chromecast state changed to: %s", state_str);
}

static void chromecast_volume_callback(const chromecast_volume_info_t* volume) {
    if (volume) {
        ESP_LOGI(TAG, "Volume callback: %.2f%% %s",
                 volume->level * 100, volume->muted ? "(Muted)" : "");
        chromecast_gui_update_volume(volume);
    }
}

static void cancel_button_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Cancel button clicked");

    if (g_gui_state.connection_modal) {
        lv_obj_del(g_gui_state.connection_modal);
        g_gui_state.connection_modal = NULL;
    }
}
