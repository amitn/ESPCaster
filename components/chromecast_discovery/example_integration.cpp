#include "chromecast_discovery.h"
#include "../chromecast_controller/chromecast_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "ChromecastIntegration";

// Example showing how to use ChromecastDiscovery with ChromecastController
void chromecast_integration_example_task(void* parameter) {
    ESP_LOGI(TAG, "Starting Chromecast integration example");
    
    // Create discovery instance
    ChromecastDiscovery discovery;
    ChromecastController controller;
    
    // Initialize discovery
    if (!discovery.initialize()) {
        ESP_LOGE(TAG, "Failed to initialize ChromecastDiscovery");
        vTaskDelete(nullptr);
        return;
    }
    
    // Initialize controller
    if (!controller.initialize()) {
        ESP_LOGE(TAG, "Failed to initialize ChromecastController");
        discovery.deinitialize();
        vTaskDelete(nullptr);
        return;
    }
    
    // Set up controller callbacks
    controller.set_state_callback([](ChromecastController::ConnectionState state) {
        switch (state) {
            case ChromecastController::DISCONNECTED:
                ESP_LOGI(TAG, "Controller State: DISCONNECTED");
                break;
            case ChromecastController::CONNECTING:
                ESP_LOGI(TAG, "Controller State: CONNECTING");
                break;
            case ChromecastController::CONNECTED:
                ESP_LOGI(TAG, "Controller State: CONNECTED");
                break;
            case ChromecastController::ERROR_STATE:
                ESP_LOGE(TAG, "Controller State: ERROR");
                break;
        }
    });
    
    controller.set_message_callback([](const std::string& namespace_str, const std::string& payload) {
        ESP_LOGI(TAG, "Message from %s: %s", namespace_str.c_str(), payload.c_str());
    });
    
    controller.set_volume_callback([](const ChromecastController::VolumeInfo& volume) {
        ESP_LOGI(TAG, "Volume update - Level: %.2f, Muted: %s", 
                volume.level, volume.muted ? "true" : "false");
    });
    
    // Discover Chromecast devices
    ESP_LOGI(TAG, "Discovering Chromecast devices...");
    std::vector<ChromecastDiscovery::DeviceInfo> devices;
    
    if (discovery.discover_devices_sync(devices)) {
        ESP_LOGI(TAG, "Discovery completed, found %d devices", devices.size());
        
        if (devices.empty()) {
            ESP_LOGW(TAG, "No Chromecast devices found");
        } else {
            // Print all found devices
            for (const auto& device : devices) {
                ESP_LOGI(TAG, "%s", ChromecastDiscovery::device_info_to_string(device).c_str());
            }
            
            // Connect to the first device found
            const auto& target_device = devices[0];
            ESP_LOGI(TAG, "Connecting to: %s", target_device.name.c_str());
            
            if (controller.connect_to_chromecast(target_device.ip_address)) {
                ESP_LOGI(TAG, "Successfully connected to Chromecast!");
                
                // Wait a bit for initial status
                vTaskDelay(pdMS_TO_TICKS(2000));
                
                // Example: Set volume to 30%
                ESP_LOGI(TAG, "Setting volume to 30%");
                controller.set_volume(0.3f, false);
                
                // Wait a bit
                vTaskDelay(pdMS_TO_TICKS(2000));
                
                // Example: Get status
                ESP_LOGI(TAG, "Requesting status");
                controller.get_status();
                
                // Keep the connection alive for demonstration
                ESP_LOGI(TAG, "Keeping connection alive for 20 seconds...");
                for (int i = 0; i < 20; i++) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    
                    // Request status every 5 seconds
                    if (i % 5 == 0) {
                        ESP_LOGI(TAG, "Requesting status...");
                        controller.get_status();
                    }
                }
                
                ESP_LOGI(TAG, "Disconnecting from Chromecast");
                controller.disconnect();
                
            } else {
                ESP_LOGE(TAG, "Failed to connect to Chromecast");
            }
        }
    } else {
        ESP_LOGE(TAG, "Discovery failed");
    }
    
    // Clean up
    discovery.deinitialize();
    
    ESP_LOGI(TAG, "Chromecast integration example completed");
    vTaskDelete(nullptr);
}

// Example with periodic discovery
void chromecast_periodic_discovery_example_task(void* parameter) {
    ESP_LOGI(TAG, "Starting periodic discovery example");
    
    ChromecastDiscovery discovery;
    
    // Set up discovery callbacks
    discovery.set_discovery_callback([](const std::vector<ChromecastDiscovery::DeviceInfo>& devices) {
        ESP_LOGI(TAG, "Discovery callback: found %d devices", devices.size());
        for (const auto& device : devices) {
            ESP_LOGI(TAG, "  - %s", ChromecastDiscovery::device_info_to_string(device).c_str());
        }
    });
    
    discovery.set_device_found_callback([](const ChromecastDiscovery::DeviceInfo& device) {
        ESP_LOGI(TAG, "Device found: %s", ChromecastDiscovery::device_info_to_string(device).c_str());
    });
    
    // Initialize and start periodic discovery
    if (discovery.initialize()) {
        ESP_LOGI(TAG, "Starting periodic discovery every 10 seconds");
        discovery.start_periodic_discovery(10000); // Every 10 seconds
        
        // Let it run for 60 seconds
        vTaskDelay(pdMS_TO_TICKS(60000));
        
        ESP_LOGI(TAG, "Stopping periodic discovery");
        discovery.stop_periodic_discovery();
        discovery.deinitialize();
    } else {
        ESP_LOGE(TAG, "Failed to initialize discovery");
    }
    
    ESP_LOGI(TAG, "Periodic discovery example completed");
    vTaskDelete(nullptr);
}

// Function to start the integration example
void start_chromecast_integration_example() {
    xTaskCreate(chromecast_integration_example_task, "chromecast_integration", 8192, nullptr, 5, nullptr);
}

// Function to start the periodic discovery example
void start_chromecast_periodic_discovery_example() {
    xTaskCreate(chromecast_periodic_discovery_example_task, "chromecast_periodic", 8192, nullptr, 5, nullptr);
}

// Example of finding a specific device by name
bool connect_to_device_by_name(const std::string& device_name) {
    ChromecastDiscovery discovery;
    ChromecastController controller;
    
    if (!discovery.initialize() || !controller.initialize()) {
        ESP_LOGE(TAG, "Failed to initialize components");
        return false;
    }
    
    std::vector<ChromecastDiscovery::DeviceInfo> devices;
    if (discovery.discover_devices_sync(devices)) {
        auto target_device = ChromecastDiscovery::find_device_by_name(devices, device_name);
        
        if (target_device.is_valid()) {
            ESP_LOGI(TAG, "Found target device: %s", 
                    ChromecastDiscovery::device_info_to_string(target_device).c_str());
            
            bool success = controller.connect_to_chromecast(target_device.ip_address);
            discovery.deinitialize();
            return success;
        } else {
            ESP_LOGW(TAG, "Device '%s' not found", device_name.c_str());
        }
    }
    
    discovery.deinitialize();
    return false;
}
