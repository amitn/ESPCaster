#pragma once

#include <string>
#include <memory>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "cJSON.h"

#include "chromecast_protobuf/cast_channel.pb-c.h"
#include "esp_tls.h"

/**
 * ChromecastController - ESP-IDF C++ class for controlling Chromecast devices
 * 
 * Features:
 * - TLS connection establishment
 * - Protobuf message handling
 * - Volume control
 * - Heartbeat/ping management
 * - JSON message serialization/deserialization
 */
class ChromecastController {
public:
    // Chromecast namespaces
    static constexpr const char* NAMESPACE_CONNECTION = "urn:x-cast:com.google.cast.tp.connection";
    static constexpr const char* NAMESPACE_HEARTBEAT = "urn:x-cast:com.google.cast.tp.heartbeat";
    static constexpr const char* NAMESPACE_RECEIVER = "urn:x-cast:com.google.cast.receiver";

    // Default Chromecast port
    static constexpr int CHROMECAST_PORT = 8009;
    static constexpr int HEARTBEAT_INTERVAL_MS = 5000;

    // Connection states
    enum ConnectionState {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        ERROR_STATE
    };

    // Volume control structure
    struct VolumeInfo {
        float level;     // 0.0 to 1.0
        bool muted;
    };

    // Callback function types
    using MessageCallback = std::function<void(const std::string&, const std::string&)>;
    using StateCallback = std::function<void(ConnectionState)>;
    using VolumeCallback = std::function<void(const VolumeInfo&)>;

private:
    // ESP-IDF specific members
    esp_tls_t* tls_handle;
    TimerHandle_t heartbeat_timer;
    TaskHandle_t receive_task_handle;

    // Connection details
    std::string chromecast_ip;
    int chromecast_port;
    std::string sender_id;
    std::string destination_id;

    // State management
    ConnectionState current_state;
    uint32_t request_id_counter;
    bool virtual_connection_established;

    // Callbacks
    MessageCallback message_callback;
    StateCallback state_callback;
    VolumeCallback volume_callback;

    // Internal methods
    bool establish_tls_connection();
    bool send_virtual_connect();
    bool send_protobuf_message(const std::string& namespace_str, const std::string& payload);
    void handle_incoming_message(const Extensions__Api__CastChannel__CastMessage* message);
    void process_receiver_message(const std::string& payload);
    std::string create_json_message(const std::string& type, uint32_t request_id = 0, const cJSON* additional_data = nullptr);

    // Memory-safe JSON parsing helper
    cJSON* safe_json_parse(const std::string& payload, size_t min_free_heap = 4096);

    // Memory management helpers
    void log_memory_status(const char* context = nullptr);

    // Static callback functions for FreeRTOS
    static void heartbeat_timer_callback(TimerHandle_t timer);
    static void receive_task(void* parameter);

public:
    ChromecastController();
    ~ChromecastController();

    // Main control methods
    bool initialize();
    bool connect_to_chromecast(const std::string& ip = "");
    void disconnect();
    bool set_volume(float level, bool muted = false);
    bool get_status();
    void start_heartbeat();
    void stop_heartbeat();

    // Callback setters
    void set_message_callback(MessageCallback callback) { message_callback = callback; }
    void set_state_callback(StateCallback callback) { state_callback = callback; }
    void set_volume_callback(VolumeCallback callback) { volume_callback = callback; }

    // Getters
    ConnectionState get_state() const { return current_state; }
    std::string get_connected_device() const { return chromecast_ip; }

    // Utility methods
    void run_message_loop();
    bool is_connected() const { return current_state == CONNECTED; }
    bool is_connection_healthy() const;
};
