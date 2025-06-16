#include "chromecast_controller_wrapper.h"
#include "chromecast_controller.h"
#include "esp_log.h"
#include <cstring>
#include <memory>
#include <new>

static const char* TAG = "chromecast_ctrl_wrapper";

// Internal wrapper structure
struct ChromecastControllerWrapper {
    std::unique_ptr<ChromecastController> controller;
    chromecast_state_callback_t state_callback;
    chromecast_volume_callback_t volume_callback;
    chromecast_message_callback_t message_callback;
    
    ChromecastControllerWrapper() : 
        state_callback(nullptr), 
        volume_callback(nullptr), 
        message_callback(nullptr) {
        controller = std::make_unique<ChromecastController>();
    }
};

// Helper function to convert C++ state to C state
static chromecast_connection_state_t convert_state(ChromecastController::ConnectionState cpp_state) {
    switch (cpp_state) {
        case ChromecastController::DISCONNECTED:
            return CHROMECAST_DISCONNECTED;
        case ChromecastController::CONNECTING:
            return CHROMECAST_CONNECTING;
        case ChromecastController::CONNECTED:
            return CHROMECAST_CONNECTED;
        case ChromecastController::ERROR_STATE:
            return CHROMECAST_ERROR_STATE;
        default:
            return CHROMECAST_ERROR_STATE;
    }
}

// Helper function to convert C++ volume to C volume
static void convert_volume_info(const ChromecastController::VolumeInfo& cpp_volume, 
                               chromecast_volume_info_t* c_volume) {
    if (!c_volume) return;
    c_volume->level = cpp_volume.level;
    c_volume->muted = cpp_volume.muted;
}

extern "C" {

chromecast_controller_handle_t chromecast_controller_create(void) {
    auto wrapper = new(std::nothrow) ChromecastControllerWrapper();
    if (!wrapper) {
        ESP_LOGE(TAG, "Failed to create ChromecastController wrapper: out of memory");
        return nullptr;
    }
    ESP_LOGI(TAG, "Created ChromecastController wrapper");
    return static_cast<chromecast_controller_handle_t>(wrapper);
}

void chromecast_controller_destroy(chromecast_controller_handle_t handle) {
    if (!handle) return;
    
    auto wrapper = static_cast<ChromecastControllerWrapper*>(handle);
    delete wrapper;
    ESP_LOGI(TAG, "Destroyed ChromecastController wrapper");
}

bool chromecast_controller_initialize(chromecast_controller_handle_t handle) {
    if (!handle) return false;
    
    auto wrapper = static_cast<ChromecastControllerWrapper*>(handle);
    
    // Set up C++ callbacks that will call C callbacks
    wrapper->controller->set_state_callback([wrapper](ChromecastController::ConnectionState state) {
        if (wrapper->state_callback) {
            wrapper->state_callback(convert_state(state));
        }
    });
    
    wrapper->controller->set_volume_callback([wrapper](const ChromecastController::VolumeInfo& volume) {
        if (wrapper->volume_callback) {
            chromecast_volume_info_t c_volume;
            convert_volume_info(volume, &c_volume);
            wrapper->volume_callback(&c_volume);
        }
    });
    
    wrapper->controller->set_message_callback([wrapper](const std::string& namespace_str, const std::string& payload) {
        if (wrapper->message_callback) {
            wrapper->message_callback(namespace_str.c_str(), payload.c_str());
        }
    });
    
    bool result = wrapper->controller->initialize();
    ESP_LOGI(TAG, "ChromecastController initialization: %s", result ? "success" : "failed");
    return result;
}

bool chromecast_controller_connect(chromecast_controller_handle_t handle, const char* ip) {
    if (!handle || !ip) return false;
    
    auto wrapper = static_cast<ChromecastControllerWrapper*>(handle);
    bool result = wrapper->controller->connect_to_chromecast(std::string(ip));
    ESP_LOGI(TAG, "ChromecastController connect to %s: %s", ip, result ? "success" : "failed");
    return result;
}

void chromecast_controller_disconnect(chromecast_controller_handle_t handle) {
    if (!handle) return;
    
    auto wrapper = static_cast<ChromecastControllerWrapper*>(handle);
    wrapper->controller->disconnect();
    ESP_LOGI(TAG, "ChromecastController disconnected");
}

bool chromecast_controller_set_volume(chromecast_controller_handle_t handle, float level, bool muted) {
    if (!handle) return false;
    
    auto wrapper = static_cast<ChromecastControllerWrapper*>(handle);
    bool result = wrapper->controller->set_volume(level, muted);
    ESP_LOGI(TAG, "ChromecastController set volume %.2f, muted: %s - %s", 
             level, muted ? "true" : "false", result ? "success" : "failed");
    return result;
}

bool chromecast_controller_get_status(chromecast_controller_handle_t handle) {
    if (!handle) return false;
    
    auto wrapper = static_cast<ChromecastControllerWrapper*>(handle);
    bool result = wrapper->controller->get_status();
    ESP_LOGI(TAG, "ChromecastController get status: %s", result ? "success" : "failed");
    return result;
}

chromecast_connection_state_t chromecast_controller_get_state(chromecast_controller_handle_t handle) {
    if (!handle) return CHROMECAST_ERROR_STATE;
    
    auto wrapper = static_cast<ChromecastControllerWrapper*>(handle);
    return convert_state(wrapper->controller->get_state());
}

bool chromecast_controller_get_connected_device(chromecast_controller_handle_t handle, 
                                               char* ip_buffer, size_t buffer_size) {
    if (!handle || !ip_buffer || buffer_size == 0) return false;
    
    auto wrapper = static_cast<ChromecastControllerWrapper*>(handle);
    std::string connected_ip = wrapper->controller->get_connected_device();
    
    if (connected_ip.empty()) {
        return false;
    }
    
    strncpy(ip_buffer, connected_ip.c_str(), buffer_size - 1);
    ip_buffer[buffer_size - 1] = '\0';
    return true;
}

void chromecast_controller_set_state_callback(chromecast_controller_handle_t handle, 
                                             chromecast_state_callback_t callback) {
    if (!handle) return;
    
    auto wrapper = static_cast<ChromecastControllerWrapper*>(handle);
    wrapper->state_callback = callback;
}

void chromecast_controller_set_volume_callback(chromecast_controller_handle_t handle, 
                                              chromecast_volume_callback_t callback) {
    if (!handle) return;
    
    auto wrapper = static_cast<ChromecastControllerWrapper*>(handle);
    wrapper->volume_callback = callback;
}

void chromecast_controller_set_message_callback(chromecast_controller_handle_t handle, 
                                               chromecast_message_callback_t callback) {
    if (!handle) return;
    
    auto wrapper = static_cast<ChromecastControllerWrapper*>(handle);
    wrapper->message_callback = callback;
}

void chromecast_controller_start_heartbeat(chromecast_controller_handle_t handle) {
    if (!handle) return;
    
    auto wrapper = static_cast<ChromecastControllerWrapper*>(handle);
    wrapper->controller->start_heartbeat();
    ESP_LOGI(TAG, "ChromecastController heartbeat started");
}

void chromecast_controller_stop_heartbeat(chromecast_controller_handle_t handle) {
    if (!handle) return;
    
    auto wrapper = static_cast<ChromecastControllerWrapper*>(handle);
    wrapper->controller->stop_heartbeat();
    ESP_LOGI(TAG, "ChromecastController heartbeat stopped");
}

} // extern "C"
