#include "chromecast_discovery.h"
#include <cstring>
#include <sstream>

extern "C" {
    #include "lvgl.h"
}

static const char* TAG = "ChromecastDiscovery";

ChromecastDiscovery::ChromecastDiscovery()
    : initialized(false)
    , discovery_active(false)
    , current_mode(SYNC_ONCE)
    , periodic_timer(nullptr)
    , periodic_interval_ms(DEFAULT_PERIODIC_INTERVAL_MS)
    , timeout_ms(DEFAULT_TIMEOUT_MS)
    , max_results(DEFAULT_MAX_RESULTS)
{
}

ChromecastDiscovery::~ChromecastDiscovery() {
    deinitialize();
}

bool ChromecastDiscovery::initialize() {
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing ChromecastDiscovery");

    // Initialize mDNS (it's OK if already initialized)
    esp_err_t err = mdns_init();
    ESP_LOGI(TAG, "mdns_init() returned: %s (0x%x)", esp_err_to_name(err), err);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS initialization failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "mDNS initialized successfully");

    initialized = true;
    ESP_LOGI(TAG, "ChromecastDiscovery initialized successfully");
    return true;
}

void ChromecastDiscovery::deinitialize() {
    if (!initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing ChromecastDiscovery");

    stop_periodic_discovery();

    if (periodic_timer) {
        xTimerDelete(periodic_timer, 0);
        periodic_timer = nullptr;
    }

    mdns_free();
    initialized = false;
    ESP_LOGI(TAG, "ChromecastDiscovery deinitialized");
}

bool ChromecastDiscovery::discover_devices_sync(std::vector<DeviceInfo>& devices, bool skip_active_check) {
    if (!initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return false;
    }

    if (!skip_active_check && discovery_active) {
        ESP_LOGW(TAG, "Discovery already active");
        return false;
    }

    // Check if WiFi is connected
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK || ip_info.ip.addr == 0) {
            ESP_LOGW(TAG, "WiFi not connected, cannot perform mDNS discovery");
            return false;
        }
    } else {
        ESP_LOGW(TAG, "WiFi interface not found, cannot perform mDNS discovery");
        return false;
    }

    ESP_LOGI(TAG, "Starting synchronous device discovery...");
    if (!skip_active_check) {
        discovery_active = true;
        current_mode = SYNC_ONCE;
    }
    devices.clear();

    // Query for Chromecast devices using mDNS
    mdns_result_t* results = nullptr;
    esp_err_t err = mdns_query_ptr(CHROMECAST_SERVICE, CHROMECAST_PROTOCOL, timeout_ms, max_results, &results);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS query failed: %s", esp_err_to_name(err));
        if (!skip_active_check) {
            discovery_active = false;
        }
        return false;
    }

    if (!results) {
        ESP_LOGW(TAG, "No Chromecast devices found");
        if (!skip_active_check) {
            discovery_active = false;
        }
        return true; // Not an error, just no devices found
    }

    // Process results
    mdns_result_t* current = results;
    while (current) {
        DeviceInfo device;
        if (parse_device_info(current, device)) {
            devices.push_back(device);
            ESP_LOGI(TAG, "Found device: %s at %s:%d",
                    device.name.c_str(), device.ip_address.c_str(), device.port);

            // Call device found callback if set
            if (device_found_callback) {
                device_found_callback(device);
            }
        }
        current = current->next;
    }

    mdns_query_results_free(results);
    if (!skip_active_check) {
        discovery_active = false;
    }

    ESP_LOGI(TAG, "Discovery completed, found %d devices", devices.size());

    // Call discovery callback if set
    if (discovery_callback) {
        discovery_callback(devices);
    }

    return true;
}

bool ChromecastDiscovery::discover_devices_async() {
    if (!initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return false;
    }

    if (discovery_active) {
        ESP_LOGW(TAG, "Discovery already active");
        return false;
    }

    // Check if WiFi is connected
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK || ip_info.ip.addr == 0) {
            ESP_LOGW(TAG, "WiFi not connected, cannot perform mDNS discovery");
            return false;
        }
    } else {
        ESP_LOGW(TAG, "WiFi interface not found, cannot perform mDNS discovery");
        return false;
    }

    ESP_LOGI(TAG, "Starting asynchronous device discovery...");
    discovery_active = true;
    current_mode = ASYNC_ONCE;

    // Create a task to run discovery asynchronously
    BaseType_t task_created = xTaskCreate(
        async_discovery_task,           // Task function
        "chromecast_discovery",         // Task name
        4096,                          // Stack size (4KB)
        this,                          // Task parameter (this instance)
        5,                             // Priority
        nullptr                        // Task handle (not needed)
    );

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create async discovery task");
        discovery_active = false;
        current_mode = SYNC_ONCE;
        return false;
    }

    return true;
}

bool ChromecastDiscovery::parse_device_info(const mdns_result_t* result, DeviceInfo& device) {
    if (!result || !result->addr) {
        return false;
    }

    // Extract IP address
    if (result->addr->addr.type == ESP_IPADDR_TYPE_V4) {
        char ip_str[16];
        esp_ip4addr_ntoa(&result->addr->addr.u_addr.ip4, ip_str, sizeof(ip_str));
        device.ip_address = std::string(ip_str);
    } else {
        ESP_LOGW(TAG, "IPv6 addresses not supported yet");
        return false;
    }
    device.port = result->port;

    // Extract instance name
    if (result->instance_name) {
        device.instance_name = std::string(result->instance_name);
        device.name = device.instance_name; // Default name to instance name
    }

    // Extract additional info from TXT records
    device.model = extract_txt_value(result, "md");
    device.uuid = extract_txt_value(result, "id");

    // Try to get friendly name from TXT records
    std::string friendly_name = extract_txt_value(result, "fn");
    if (!friendly_name.empty()) {
        device.name = friendly_name;
    }

    return device.is_valid();
}

std::string ChromecastDiscovery::extract_txt_value(const mdns_result_t* result, const char* key) {
    if (!result || !result->txt || !key || result->txt_count == 0) {
        return "";
    }

    // Iterate through TXT records using array indexing instead of linked list
    for (size_t i = 0; i < result->txt_count; i++) {
        if (result->txt[i].key && strcmp(result->txt[i].key, key) == 0 && result->txt[i].value) {
            return std::string(result->txt[i].value);
        }
    }
    return "";
}

bool ChromecastDiscovery::start_periodic_discovery(uint32_t interval_ms) {
    if (!initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return false;
    }

    if (current_mode == PERIODIC) {
        ESP_LOGW(TAG, "Periodic discovery already running");
        return true;
    }

    ESP_LOGI(TAG, "Starting periodic discovery with interval %d ms", interval_ms);

    periodic_interval_ms = interval_ms;
    current_mode = PERIODIC;

    // Update timer period
    xTimerChangePeriod(periodic_timer, pdMS_TO_TICKS(periodic_interval_ms), 0);

    // Start the timer
    if (xTimerStart(periodic_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start periodic timer");
        current_mode = SYNC_ONCE;
        return false;
    }

    return true;
}

void ChromecastDiscovery::stop_periodic_discovery() {
    if (current_mode == PERIODIC && periodic_timer) {
        ESP_LOGI(TAG, "Stopping periodic discovery");
        xTimerStop(periodic_timer, 0);
        current_mode = SYNC_ONCE;
    }
}

void ChromecastDiscovery::periodic_timer_callback(TimerHandle_t timer) {
    ChromecastDiscovery* discovery = static_cast<ChromecastDiscovery*>(pvTimerGetTimerID(timer));

    if (discovery && discovery->initialized && discovery->current_mode == PERIODIC) {
        ESP_LOGD(TAG, "Periodic discovery triggered");

        std::vector<DeviceInfo> devices;
        discovery->discover_devices_sync(devices);
    }
}

// Structure to pass data to the main thread callback
struct AsyncCallbackData {
    ChromecastDiscovery* discovery;
    std::vector<ChromecastDiscovery::DeviceInfo>* devices;
    bool success;

    AsyncCallbackData(ChromecastDiscovery* disc, std::vector<ChromecastDiscovery::DeviceInfo>* devs, bool succ)
        : discovery(disc), devices(devs), success(succ) {}
};

// Callback function that runs in the main LVGL thread
void ChromecastDiscovery::async_callback_main_thread(void* user_data) {
    AsyncCallbackData* data = static_cast<AsyncCallbackData*>(user_data);

    if (!data || !data->discovery) {
        ESP_LOGE(TAG, "Invalid callback data");
        delete data;
        return;
    }

    ChromecastDiscovery* discovery = data->discovery;

    if (data->success && data->devices) {
        ESP_LOGI(TAG, "Triggering callbacks from main thread for %d devices", data->devices->size());

        // Call discovery callback if set (this will update the GUI)
        if (discovery->discovery_callback) {
            discovery->discovery_callback(*data->devices);
        }
    } else {
        ESP_LOGE(TAG, "Discovery failed, not triggering callbacks");
    }

    // Cleanup
    delete data->devices;
    delete data;
}

void ChromecastDiscovery::async_discovery_task(void* parameter) {
    ChromecastDiscovery* discovery = static_cast<ChromecastDiscovery*>(parameter);

    if (!discovery || !discovery->initialized) {
        ESP_LOGE(TAG, "Invalid discovery instance in async task");
        if (discovery) {
            discovery->discovery_active = false;
        }
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "Async discovery task started");

    // Perform the actual discovery
    std::vector<ChromecastDiscovery::DeviceInfo>* devices = new std::vector<ChromecastDiscovery::DeviceInfo>();
    bool result = false;

    // Temporarily call the sync version but without triggering callbacks
    // We'll trigger them from the main thread instead
    auto original_callback = discovery->discovery_callback;
    auto original_device_callback = discovery->device_found_callback;

    // Temporarily disable callbacks during sync discovery
    discovery->discovery_callback = nullptr;
    discovery->device_found_callback = nullptr;

    result = discovery->discover_devices_sync(*devices, true); // Skip active check since we're already in async mode

    // Restore original callbacks
    discovery->discovery_callback = original_callback;
    discovery->device_found_callback = original_device_callback;

    // Mark discovery as no longer active
    discovery->discovery_active = false;

    if (!result) {
        ESP_LOGE(TAG, "Async discovery failed");
        delete devices;
    } else {
        ESP_LOGI(TAG, "Async discovery completed successfully, found %d devices", devices->size());

        // Schedule callback to run in main thread using LVGL's async mechanism
        AsyncCallbackData* callback_data = new AsyncCallbackData(discovery, devices, result);

        // Use LVGL's async call to execute the callback in the main thread
        if (lv_async_call(async_callback_main_thread, callback_data) != LV_RES_OK) {
            ESP_LOGE(TAG, "Failed to schedule async callback");
            delete devices;
            delete callback_data;
        }
    }

    // Task cleanup - delete itself
    vTaskDelete(nullptr);
}

std::string ChromecastDiscovery::device_info_to_string(const DeviceInfo& device) {
    std::stringstream ss;
    ss << "Device: " << device.name
       << " (" << device.instance_name << ")"
       << " at " << device.ip_address << ":" << device.port;

    if (!device.model.empty()) {
        ss << " [Model: " << device.model << "]";
    }

    if (!device.uuid.empty()) {
        ss << " [UUID: " << device.uuid << "]";
    }

    return ss.str();
}

ChromecastDiscovery::DeviceInfo ChromecastDiscovery::find_device_by_name(
    const std::vector<DeviceInfo>& devices, const std::string& name) {

    for (const auto& device : devices) {
        if (device.name == name || device.instance_name == name) {
            return device;
        }
    }
    return DeviceInfo(); // Return invalid device if not found
}

ChromecastDiscovery::DeviceInfo ChromecastDiscovery::find_device_by_ip(
    const std::vector<DeviceInfo>& devices, const std::string& ip) {

    for (const auto& device : devices) {
        if (device.ip_address == ip) {
            return device;
        }
    }
    return DeviceInfo(); // Return invalid device if not found
}


