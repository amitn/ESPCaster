#include "chromecast_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "ChromecastExample";

// Example usage of the ChromecastController
void chromecast_example_task(void* parameter) {
    ESP_LOGI(TAG, "Starting Chromecast example");
    
    // Create controller instance
    ChromecastController controller;
    
    // Set up callbacks
    controller.set_state_callback([](ChromecastController::ConnectionState state) {
        switch (state) {
            case ChromecastController::DISCONNECTED:
                ESP_LOGI(TAG, "State: DISCONNECTED");
                break;
            case ChromecastController::CONNECTING:
                ESP_LOGI(TAG, "State: CONNECTING");
                break;
            case ChromecastController::CONNECTED:
                ESP_LOGI(TAG, "State: CONNECTED");
                break;
            case ChromecastController::ERROR_STATE:
                ESP_LOGE(TAG, "State: ERROR");
                break;
        }
    });
    
    controller.set_message_callback([](const std::string& namespace_str, const std::string& payload) {
        ESP_LOGI(TAG, "Message callback - Namespace: %s", namespace_str.c_str());
        ESP_LOGD(TAG, "Message payload: %s", payload.c_str());
    });
    
    controller.set_volume_callback([](const ChromecastController::VolumeInfo& volume) {
        ESP_LOGI(TAG, "Volume update - Level: %.2f, Muted: %s", 
                volume.level, volume.muted ? "true" : "false");
    });
    
    // Initialize the controller
    if (!controller.initialize()) {
        ESP_LOGE(TAG, "Failed to initialize ChromecastController");
        vTaskDelete(nullptr);
        return;
    }
    
    // Connect to a specific Chromecast device
    // Replace "192.168.1.100" with your Chromecast IP address
    const char* chromecast_ip = "192.168.1.100";
    
    ESP_LOGI(TAG, "Attempting to connect to Chromecast at %s", chromecast_ip);
    
    if (controller.connect_to_chromecast(chromecast_ip)) {
        ESP_LOGI(TAG, "Successfully connected to Chromecast!");
        
        // Wait a bit for initial status
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // Example: Set volume to 50%
        ESP_LOGI(TAG, "Setting volume to 50%");
        controller.set_volume(0.5f, false);
        
        // Wait a bit
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // Example: Mute the device
        ESP_LOGI(TAG, "Muting device");
        controller.set_volume(0.5f, true);
        
        // Wait a bit
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // Example: Unmute the device
        ESP_LOGI(TAG, "Unmuting device");
        controller.set_volume(0.5f, false);
        
        // Keep the connection alive for demonstration
        ESP_LOGI(TAG, "Keeping connection alive for 30 seconds...");
        for (int i = 0; i < 30; i++) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            
            // Request status every 10 seconds
            if (i % 10 == 0) {
                ESP_LOGI(TAG, "Requesting status...");
                controller.get_status();
            }
        }
        
        ESP_LOGI(TAG, "Disconnecting from Chromecast");
        controller.disconnect();
        
    } else {
        ESP_LOGE(TAG, "Failed to connect to Chromecast");
    }
    
    ESP_LOGI(TAG, "Chromecast example completed");
    vTaskDelete(nullptr);
}

// Function to start the example
void start_chromecast_example() {
    xTaskCreate(chromecast_example_task, "chromecast_example", 8192, nullptr, 5, nullptr);
}
