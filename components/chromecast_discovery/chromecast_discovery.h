#pragma once

#include <string>
#include <vector>
#include <functional>
#include "esp_log.h"
#include "esp_netif.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

/**
 * ChromecastDiscovery - ESP-IDF C++ class for discovering Chromecast devices via mDNS
 * 
 * Features:
 * - Synchronous and asynchronous mDNS discovery
 * - Device information extraction
 * - Callback-based notifications
 * - Automatic periodic discovery
 */
class ChromecastDiscovery {
public:
    // Chromecast device information
    struct DeviceInfo {
        std::string name;           // Device friendly name
        std::string ip_address;     // IP address as string
        int port;                   // Port number (usually 8009)
        std::string instance_name;  // mDNS instance name
        std::string model;          // Device model (if available)
        std::string uuid;           // Device UUID (if available)
        
        DeviceInfo() : port(8009) {}
        
        bool is_valid() const {
            return !ip_address.empty() && port > 0;
        }
    };

    // Discovery result callback
    using DiscoveryCallback = std::function<void(const std::vector<DeviceInfo>&)>;
    using DeviceFoundCallback = std::function<void(const DeviceInfo&)>;

    // Discovery modes
    enum DiscoveryMode {
        SYNC_ONCE,      // Single synchronous discovery
        ASYNC_ONCE,     // Single asynchronous discovery
        PERIODIC        // Periodic discovery with timer
    };

private:
    // mDNS search parameters
    static constexpr const char* CHROMECAST_SERVICE = "_googlecast";
    static constexpr const char* CHROMECAST_PROTOCOL = "_tcp";
    static constexpr uint32_t DEFAULT_TIMEOUT_MS = 3000;
    static constexpr size_t DEFAULT_MAX_RESULTS = 20;
    static constexpr uint32_t DEFAULT_PERIODIC_INTERVAL_MS = 30000; // 30 seconds

    // Internal state
    bool initialized;
    bool discovery_active;
    DiscoveryMode current_mode;
    
    // Callbacks
    DiscoveryCallback discovery_callback;
    DeviceFoundCallback device_found_callback;
    
    // Periodic discovery
    TimerHandle_t periodic_timer;
    uint32_t periodic_interval_ms;
    
    // Discovery parameters
    uint32_t timeout_ms;
    size_t max_results;
    
    // Internal methods
    bool parse_device_info(const mdns_result_t* result, DeviceInfo& device);
    std::string extract_txt_value(const mdns_result_t* result, const char* key);
    void process_discovery_results(mdns_result_t* results);
    
    // Static callback for periodic timer
    static void periodic_timer_callback(TimerHandle_t timer);

    // Static task function for async discovery
    static void async_discovery_task(void* parameter);

    // Static callback function for main thread execution
    static void async_callback_main_thread(void* user_data);

public:
    ChromecastDiscovery();
    ~ChromecastDiscovery();

    // Initialization
    bool initialize();
    void deinitialize();

    // Discovery methods
    bool discover_devices_sync(std::vector<DeviceInfo>& devices, bool skip_active_check = false);
    bool discover_devices_async();
    bool start_periodic_discovery(uint32_t interval_ms = DEFAULT_PERIODIC_INTERVAL_MS);
    void stop_periodic_discovery();

    // Configuration
    void set_timeout(uint32_t timeout_ms) { this->timeout_ms = timeout_ms; }
    void set_max_results(size_t max_results) { this->max_results = max_results; }
    void set_periodic_interval(uint32_t interval_ms) { this->periodic_interval_ms = interval_ms; }

    // Callbacks
    void set_discovery_callback(DiscoveryCallback callback) { discovery_callback = callback; }
    void set_device_found_callback(DeviceFoundCallback callback) { device_found_callback = callback; }

    // Getters
    bool is_initialized() const { return initialized; }
    bool is_discovery_active() const { return discovery_active; }
    DiscoveryMode get_current_mode() const { return current_mode; }

    // Utility methods
    static std::string device_info_to_string(const DeviceInfo& device);
    static DeviceInfo find_device_by_name(const std::vector<DeviceInfo>& devices, const std::string& name);
    static DeviceInfo find_device_by_ip(const std::vector<DeviceInfo>& devices, const std::string& ip);
};
