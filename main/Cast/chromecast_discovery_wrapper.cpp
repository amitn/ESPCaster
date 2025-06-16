#include "chromecast_discovery_wrapper.h"
#include "chromecast_discovery.h"
#include "esp_log.h"
#include <cstring>
#include <memory>
#include <new>

static const char* TAG = "chromecast_wrapper";

// Internal wrapper structure
struct ChromecastDiscoveryWrapper {
    std::unique_ptr<ChromecastDiscovery> discovery;
    chromecast_discovery_callback_t discovery_callback;
    chromecast_device_found_callback_t device_found_callback;
    
    ChromecastDiscoveryWrapper() : discovery_callback(nullptr), device_found_callback(nullptr) {
        discovery = std::make_unique<ChromecastDiscovery>();
    }
};

// Helper function to convert C++ DeviceInfo to C struct
static void convert_device_info(const ChromecastDiscovery::DeviceInfo& cpp_device, 
                               chromecast_device_info_t* c_device) {
    if (!c_device) return;
    
    strncpy(c_device->name, cpp_device.name.c_str(), sizeof(c_device->name) - 1);
    c_device->name[sizeof(c_device->name) - 1] = '\0';
    
    strncpy(c_device->ip_address, cpp_device.ip_address.c_str(), sizeof(c_device->ip_address) - 1);
    c_device->ip_address[sizeof(c_device->ip_address) - 1] = '\0';
    
    c_device->port = cpp_device.port;
    
    strncpy(c_device->instance_name, cpp_device.instance_name.c_str(), sizeof(c_device->instance_name) - 1);
    c_device->instance_name[sizeof(c_device->instance_name) - 1] = '\0';
    
    strncpy(c_device->model, cpp_device.model.c_str(), sizeof(c_device->model) - 1);
    c_device->model[sizeof(c_device->model) - 1] = '\0';
    
    strncpy(c_device->uuid, cpp_device.uuid.c_str(), sizeof(c_device->uuid) - 1);
    c_device->uuid[sizeof(c_device->uuid) - 1] = '\0';
}

extern "C" {

chromecast_discovery_handle_t chromecast_discovery_create(void) {
    auto wrapper = new(std::nothrow) ChromecastDiscoveryWrapper();
    if (!wrapper) {
        ESP_LOGE(TAG, "Failed to create ChromecastDiscovery wrapper: out of memory");
        return nullptr;
    }
    ESP_LOGI(TAG, "Created ChromecastDiscovery wrapper");
    return static_cast<chromecast_discovery_handle_t>(wrapper);
}

void chromecast_discovery_destroy(chromecast_discovery_handle_t handle) {
    if (!handle) return;
    
    auto wrapper = static_cast<ChromecastDiscoveryWrapper*>(handle);
    delete wrapper;
    ESP_LOGI(TAG, "Destroyed ChromecastDiscovery wrapper");
}

bool chromecast_discovery_initialize(chromecast_discovery_handle_t handle) {
    if (!handle) return false;
    
    auto wrapper = static_cast<ChromecastDiscoveryWrapper*>(handle);
    
    // Set up C++ callbacks that will call C callbacks
    wrapper->discovery->set_discovery_callback([wrapper](const std::vector<ChromecastDiscovery::DeviceInfo>& devices) {
        if (wrapper->discovery_callback) {
            // Convert C++ vector to C array
            auto c_devices = std::make_unique<chromecast_device_info_t[]>(devices.size());
            for (size_t i = 0; i < devices.size(); i++) {
                convert_device_info(devices[i], &c_devices[i]);
            }
            wrapper->discovery_callback(c_devices.get(), devices.size());
        }
    });
    
    wrapper->discovery->set_device_found_callback([wrapper](const ChromecastDiscovery::DeviceInfo& device) {
        if (wrapper->device_found_callback) {
            chromecast_device_info_t c_device;
            convert_device_info(device, &c_device);
            wrapper->device_found_callback(&c_device);
        }
    });
    
    bool result = wrapper->discovery->initialize();
    ESP_LOGI(TAG, "ChromecastDiscovery initialization: %s", result ? "success" : "failed");
    return result;
}

void chromecast_discovery_deinitialize(chromecast_discovery_handle_t handle) {
    if (!handle) return;
    
    auto wrapper = static_cast<ChromecastDiscoveryWrapper*>(handle);
    wrapper->discovery->deinitialize();
    ESP_LOGI(TAG, "ChromecastDiscovery deinitialized");
}

bool chromecast_discovery_discover_sync(chromecast_discovery_handle_t handle,
                                       chromecast_device_info_t* devices,
                                       size_t max_devices,
                                       size_t* device_count) {
    if (!handle || !devices || !device_count) return false;
    
    auto wrapper = static_cast<ChromecastDiscoveryWrapper*>(handle);
    std::vector<ChromecastDiscovery::DeviceInfo> cpp_devices;
    
    bool result = wrapper->discovery->discover_devices_sync(cpp_devices);
    if (!result) {
        *device_count = 0;
        return false;
    }
    
    // Convert results to C format
    size_t count = std::min(cpp_devices.size(), max_devices);
    for (size_t i = 0; i < count; i++) {
        convert_device_info(cpp_devices[i], &devices[i]);
    }
    
    *device_count = count;
    ESP_LOGI(TAG, "Synchronous discovery completed, found %d devices", count);
    return true;
}

bool chromecast_discovery_discover_async(chromecast_discovery_handle_t handle) {
    if (!handle) return false;
    
    auto wrapper = static_cast<ChromecastDiscoveryWrapper*>(handle);
    bool result = wrapper->discovery->discover_devices_async();
    ESP_LOGI(TAG, "Asynchronous discovery started: %s", result ? "success" : "failed");
    return result;
}

bool chromecast_discovery_start_periodic(chromecast_discovery_handle_t handle, uint32_t interval_ms) {
    if (!handle) return false;
    
    auto wrapper = static_cast<ChromecastDiscoveryWrapper*>(handle);
    bool result = wrapper->discovery->start_periodic_discovery(interval_ms);
    ESP_LOGI(TAG, "Periodic discovery started with interval %d ms: %s", 
             interval_ms, result ? "success" : "failed");
    return result;
}

void chromecast_discovery_stop_periodic(chromecast_discovery_handle_t handle) {
    if (!handle) return;
    
    auto wrapper = static_cast<ChromecastDiscoveryWrapper*>(handle);
    wrapper->discovery->stop_periodic_discovery();
    ESP_LOGI(TAG, "Periodic discovery stopped");
}

void chromecast_discovery_set_timeout(chromecast_discovery_handle_t handle, uint32_t timeout_ms) {
    if (!handle) return;
    
    auto wrapper = static_cast<ChromecastDiscoveryWrapper*>(handle);
    wrapper->discovery->set_timeout(timeout_ms);
}

void chromecast_discovery_set_max_results(chromecast_discovery_handle_t handle, size_t max_results) {
    if (!handle) return;
    
    auto wrapper = static_cast<ChromecastDiscoveryWrapper*>(handle);
    wrapper->discovery->set_max_results(max_results);
}

void chromecast_discovery_set_callback(chromecast_discovery_handle_t handle, 
                                      chromecast_discovery_callback_t callback) {
    if (!handle) return;
    
    auto wrapper = static_cast<ChromecastDiscoveryWrapper*>(handle);
    wrapper->discovery_callback = callback;
}

void chromecast_discovery_set_device_found_callback(chromecast_discovery_handle_t handle,
                                                   chromecast_device_found_callback_t callback) {
    if (!handle) return;
    
    auto wrapper = static_cast<ChromecastDiscoveryWrapper*>(handle);
    wrapper->device_found_callback = callback;
}

bool chromecast_discovery_is_initialized(chromecast_discovery_handle_t handle) {
    if (!handle) return false;
    
    auto wrapper = static_cast<ChromecastDiscoveryWrapper*>(handle);
    return wrapper->discovery->is_initialized();
}

bool chromecast_discovery_is_active(chromecast_discovery_handle_t handle) {
    if (!handle) return false;
    
    auto wrapper = static_cast<ChromecastDiscoveryWrapper*>(handle);
    return wrapper->discovery->is_discovery_active();
}

bool chromecast_device_info_to_string(const chromecast_device_info_t* device, char* buffer, size_t buffer_size) {
    if (!device || !buffer || buffer_size == 0) return false;
    
    int written = snprintf(buffer, buffer_size, 
                          "Device: %s (%s) at %s:%d [Model: %s, UUID: %s]",
                          device->name, device->instance_name, device->ip_address, 
                          device->port, device->model, device->uuid);
    
    return written > 0 && written < (int)buffer_size;
}

} // extern "C"
