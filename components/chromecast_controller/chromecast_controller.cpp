#include "chromecast_controller.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"

static const char* TAG = "ChromecastController";

ChromecastController::ChromecastController() 
    : tls_handle(nullptr)
    , heartbeat_timer(nullptr)
    , receive_task_handle(nullptr)
    , chromecast_port(CHROMECAST_PORT)
    , sender_id("sender-0")
    , destination_id("receiver-0")
    , current_state(DISCONNECTED)
    , request_id_counter(1)
    , virtual_connection_established(false)
{
}

ChromecastController::~ChromecastController() {
    disconnect();
}

bool ChromecastController::initialize() {
    ESP_LOGI(TAG, "Initializing ChromecastController");

    // Create heartbeat timer
    heartbeat_timer = xTimerCreate(
        "heartbeat_timer",
        pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS),
        pdTRUE,  // Auto-reload
        this,    // Timer ID (pass this instance)
        heartbeat_timer_callback
    );

    if (heartbeat_timer == nullptr) {
        ESP_LOGE(TAG, "Failed to create heartbeat timer");
        return false;
    }

    current_state = DISCONNECTED;
    ESP_LOGI(TAG, "ChromecastController initialized successfully");
    return true;
}

bool ChromecastController::establish_tls_connection() {
    ESP_LOGI(TAG, "Establishing TLS connection to %s:%d (skipping certificate verification)", chromecast_ip.c_str(), chromecast_port);

    esp_tls_cfg_t cfg = {};
    cfg.timeout_ms = 10000;
    cfg.use_secure_element = false;
    cfg.skip_common_name = true;
    // Skip certificate verification for Chromecast connections
    // This is necessary because Chromecast devices use self-signed certificates
    // With CONFIG_ESP_TLS_INSECURE=y and CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY=y,
    // setting crt_bundle_attach to nullptr will skip certificate verification
    cfg.crt_bundle_attach = nullptr;

    tls_handle = esp_tls_init();
    if (!tls_handle) {
        ESP_LOGE(TAG, "Failed to initialize TLS handle");
        current_state = ERROR_STATE;
        if (state_callback) state_callback(current_state);
        return false;
    }

    int ret = esp_tls_conn_new_sync(chromecast_ip.c_str(), chromecast_ip.length(), chromecast_port, &cfg, tls_handle);
    if (ret != 1) {
        ESP_LOGE(TAG, "Failed to establish TLS connection, ret=%d", ret);
        esp_tls_conn_destroy(tls_handle);
        tls_handle = nullptr;
        current_state = ERROR_STATE;
        if (state_callback) state_callback(current_state);
        return false;
    }

    ESP_LOGI(TAG, "TLS connection established successfully");
    current_state = CONNECTING;
    if (state_callback) state_callback(current_state);
    return true;
}

bool ChromecastController::send_virtual_connect() {
    ESP_LOGI(TAG, "Sending virtual CONNECT message");

    // Create CONNECT message
    std::string connect_msg = create_json_message("CONNECT");

    bool result = send_protobuf_message(NAMESPACE_CONNECTION, connect_msg);
    if (result) {
        virtual_connection_established = true;
        current_state = CONNECTED;
        if (state_callback) state_callback(current_state);
        ESP_LOGI(TAG, "Virtual connection established successfully");
    } else {
        ESP_LOGE(TAG, "Failed to send virtual CONNECT message");
    }

    return result;
}

bool ChromecastController::send_protobuf_message(const std::string& namespace_str, const std::string& payload) {
    if (!tls_handle) {
        ESP_LOGE(TAG, "TLS connection not established");
        return false;
    }

    // Create protobuf message
    Extensions__Api__CastChannel__CastMessage message = EXTENSIONS__API__CAST_CHANNEL__CAST_MESSAGE__INIT;

    message.protocol_version = EXTENSIONS__API__CAST_CHANNEL__CAST_MESSAGE__PROTOCOL_VERSION__CASTV2_1_0;
    message.source_id = const_cast<char*>(sender_id.c_str());
    message.destination_id = const_cast<char*>(destination_id.c_str());
    message.namespace_ = const_cast<char*>(namespace_str.c_str());
    message.payload_type = EXTENSIONS__API__CAST_CHANNEL__CAST_MESSAGE__PAYLOAD_TYPE__STRING;
    message.payload_utf8 = const_cast<char*>(payload.c_str());

    // Serialize the message
    size_t message_size = extensions__api__cast_channel__cast_message__get_packed_size(&message);
    uint8_t* buffer = (uint8_t*)malloc(message_size + 4); // +4 for length prefix

    if (!buffer) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return false;
    }

    // Pack message size (big-endian)
    buffer[0] = (message_size >> 24) & 0xFF;
    buffer[1] = (message_size >> 16) & 0xFF;
    buffer[2] = (message_size >> 8) & 0xFF;
    buffer[3] = message_size & 0xFF;

    // Pack the protobuf message
    extensions__api__cast_channel__cast_message__pack(&message, buffer + 4);

    // Send via TLS
    size_t total_size = message_size + 4;
    ssize_t sent = esp_tls_conn_write(tls_handle, buffer, total_size);

    free(buffer);

    if (sent != total_size) {
        ESP_LOGE(TAG, "Failed to send message: sent %d of %d bytes", sent, total_size);
        return false;
    }

    ESP_LOGI(TAG, "SENT -> Namespace: %s, Size: %d bytes", namespace_str.c_str(), total_size);
    ESP_LOGD(TAG, "SENT -> Payload: %s", payload.c_str());
    return true;
}

std::string ChromecastController::create_json_message(const std::string& type, uint32_t request_id, const cJSON* additional_data) {
    // Check available memory before creating JSON
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < 8192) { // Require at least 8KB free heap
        ESP_LOGW(TAG, "Insufficient memory for JSON creation: %d bytes available", free_heap);
        return "{}"; // Return minimal valid JSON
    }

    cJSON* json = cJSON_CreateObject();
    if (!json) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return "{}";
    }

    // Add type field
    if (!cJSON_AddStringToObject(json, "type", type.c_str())) {
        ESP_LOGE(TAG, "Failed to add type to JSON");
        cJSON_Delete(json);
        return "{}";
    }

    // Add request ID
    if (request_id == 0) {
        request_id = request_id_counter++;
    }
    if (!cJSON_AddNumberToObject(json, "requestId", request_id)) {
        ESP_LOGE(TAG, "Failed to add requestId to JSON");
        cJSON_Delete(json);
        return "{}";
    }

    // Add additional data if provided
    if (additional_data) {
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, additional_data) {
            if (item->string) {
                cJSON* duplicate = cJSON_Duplicate(item, true);
                if (duplicate) {
                    if (!cJSON_AddItemToObject(json, item->string, duplicate)) {
                        ESP_LOGW(TAG, "Failed to add item %s to JSON", item->string);
                        cJSON_Delete(duplicate);
                    }
                }
            }
        }
    }

    char* json_string = cJSON_Print(json);
    std::string result;

    if (json_string) {
        result = std::string(json_string);
        cJSON_free(json_string);
    } else {
        ESP_LOGE(TAG, "Failed to print JSON to string");
        result = "{}";
    }

    cJSON_Delete(json);
    return result;
}

bool ChromecastController::connect_to_chromecast(const std::string& ip) {
    ESP_LOGI(TAG, "Connecting to Chromecast...");
    log_memory_status("Before connection");

    if (ip.empty()) {
        ESP_LOGI(TAG, "No IP provided, attempting discovery...");
        return false;
    }

    chromecast_ip = ip;
    chromecast_port = CHROMECAST_PORT; // Use default port
    ESP_LOGI(TAG, "Using provided IP: %s:%d", chromecast_ip.c_str(), chromecast_port);
    
    // Establish TLS connection
    if (!establish_tls_connection()) {
        return false;
    }

    // Send virtual connect
    if (!send_virtual_connect()) {
        return false;
    }

    // Start receive task with larger stack size
    xTaskCreate(receive_task, "chromecast_receive", 8192, this, 5, &receive_task_handle);

    // Start heartbeat
    start_heartbeat();

    // Get initial status
    get_status();

    log_memory_status("After connection");
    ESP_LOGI(TAG, "Successfully connected to Chromecast at %s:%d", chromecast_ip.c_str(), chromecast_port);
    return true;
}

void ChromecastController::disconnect() {
    ESP_LOGI(TAG, "Disconnecting from Chromecast");

    stop_heartbeat();

    // Send CLOSE message if connected
    if (virtual_connection_established) {
        std::string close_msg = create_json_message("CLOSE");
        send_protobuf_message(NAMESPACE_CONNECTION, close_msg);
        virtual_connection_established = false;
    }

    // Clean up receive task
    if (receive_task_handle) {
        vTaskDelete(receive_task_handle);
        receive_task_handle = nullptr;
    }

    // Close TLS connection
    if (tls_handle) {
        esp_tls_conn_destroy(tls_handle);
        tls_handle = nullptr;
    }

    current_state = DISCONNECTED;
    if (state_callback) state_callback(current_state);
    ESP_LOGI(TAG, "Disconnected from Chromecast");
}

bool ChromecastController::set_volume(float level, bool muted) {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Chromecast");
        return false;
    }

    // Check available memory before creating JSON
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < 8192) { // Require at least 8KB free heap
        ESP_LOGW(TAG, "Insufficient memory for volume command: %d bytes available", free_heap);
        return false;
    }

    // Clamp volume level between 0.0 and 1.0
    level = std::max(0.0f, std::min(1.0f, level));

    ESP_LOGI(TAG, "Setting volume to %.2f, muted: %s", level, muted ? "true" : "false");

    // Create volume object
    cJSON* volume = cJSON_CreateObject();
    if (!volume) {
        ESP_LOGE(TAG, "Failed to create volume JSON object");
        return false;
    }

    if (!cJSON_AddNumberToObject(volume, "level", level)) {
        ESP_LOGE(TAG, "Failed to add level to volume JSON");
        cJSON_Delete(volume);
        return false;
    }

    if (!cJSON_AddBoolToObject(volume, "muted", muted)) {
        ESP_LOGE(TAG, "Failed to add muted to volume JSON");
        cJSON_Delete(volume);
        return false;
    }

    // Create SET_VOLUME message
    cJSON* message_data = cJSON_CreateObject();
    if (!message_data) {
        ESP_LOGE(TAG, "Failed to create message data JSON object");
        cJSON_Delete(volume);
        return false;
    }

    if (!cJSON_AddItemToObject(message_data, "volume", volume)) {
        ESP_LOGE(TAG, "Failed to add volume to message data");
        cJSON_Delete(volume);
        cJSON_Delete(message_data);
        return false;
    }

    std::string set_volume_msg = create_json_message("SET_VOLUME", 0, message_data);

    cJSON_Delete(message_data);

    return send_protobuf_message(NAMESPACE_RECEIVER, set_volume_msg);
}

bool ChromecastController::get_status() {
    if (!is_connected()) {
        ESP_LOGE(TAG, "Not connected to Chromecast");
        return false;
    }

    ESP_LOGD(TAG, "Requesting status");
    std::string get_status_msg = create_json_message("GET_STATUS");
    return send_protobuf_message(NAMESPACE_RECEIVER, get_status_msg);
}

void ChromecastController::start_heartbeat() {
    if (heartbeat_timer) {
        ESP_LOGI(TAG, "Starting heartbeat");
        xTimerStart(heartbeat_timer, 0);
    }
}

void ChromecastController::stop_heartbeat() {
    if (heartbeat_timer) {
        ESP_LOGI(TAG, "Stopping heartbeat");
        xTimerStop(heartbeat_timer, 0);
    }
}

void ChromecastController::heartbeat_timer_callback(TimerHandle_t timer) {
    ChromecastController* controller = static_cast<ChromecastController*>(pvTimerGetTimerID(timer));

    if (controller && controller->is_connection_healthy()) {
        // Check memory before sending heartbeat
        size_t free_heap = esp_get_free_heap_size();
        if (free_heap < 16384) { // Less than 16KB
            ESP_LOGW(TAG, "Skipping heartbeat due to low memory: %d bytes", free_heap);
            return;
        }

        ESP_LOGD(TAG, "Sending heartbeat PING");
        std::string ping_msg = controller->create_json_message("PING");
        bool success = controller->send_protobuf_message(NAMESPACE_HEARTBEAT, ping_msg);

        if (!success) {
            ESP_LOGW(TAG, "Failed to send heartbeat PING - connection may be lost");
            // Note: Connection state will be updated by the receive task if it fails
        }
    } else if (controller) {
        ESP_LOGD(TAG, "Skipping heartbeat - connection not healthy (state: %d, tls_handle: %p, virtual_connection: %s)",
                 controller->current_state, controller->tls_handle,
                 controller->virtual_connection_established ? "true" : "false");
    }
}

void ChromecastController::receive_task(void* parameter) {
    ChromecastController* controller = static_cast<ChromecastController*>(parameter);

    // Allocate buffer on heap instead of stack to prevent stack overflow
    uint8_t* buffer = (uint8_t*)malloc(2048);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate receive buffer");
        vTaskDelete(nullptr);
        return;
    }

    int consecutive_errors = 0;
    const int MAX_CONSECUTIVE_ERRORS = 5;
    int message_count = 0;

    ESP_LOGI(TAG, "Receive task started - Free heap: %d bytes", esp_get_free_heap_size());

    while (controller->is_connected() && consecutive_errors < MAX_CONSECUTIVE_ERRORS) {
        // Read message length (4 bytes, big-endian)
        ssize_t len_read = esp_tls_conn_read(controller->tls_handle, buffer, 4);
        if (len_read != 4) {
            if (len_read == 0) {
                ESP_LOGW(TAG, "Connection closed by remote");
            } else if (len_read < 0) {
                ESP_LOGE(TAG, "TLS read error: %d", len_read);
            } else {
                ESP_LOGE(TAG, "Incomplete length read: %d bytes", len_read);
            }
            consecutive_errors++;
            vTaskDelay(pdMS_TO_TICKS(100)); // Brief delay before retry
            continue;
        }

        uint32_t message_length = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];

        if (message_length == 0) {
            ESP_LOGW(TAG, "Received zero-length message");
            consecutive_errors++;
            continue;
        }

        if (message_length > 2048) {
            ESP_LOGE(TAG, "Message too large: %d bytes (max: 2048)", message_length);
            consecutive_errors++;
            continue;
        }

        // Read message data
        ssize_t data_read = esp_tls_conn_read(controller->tls_handle, buffer, message_length);
        if (data_read != message_length) {
            if (data_read < 0) {
                ESP_LOGE(TAG, "TLS read error for message data: %d", data_read);
            } else {
                ESP_LOGE(TAG, "Incomplete message read: %d of %d bytes", data_read, message_length);
            }
            consecutive_errors++;
            vTaskDelay(pdMS_TO_TICKS(100)); // Brief delay before retry
            continue;
        }

        // Unpack protobuf message
        Extensions__Api__CastChannel__CastMessage* message =
            extensions__api__cast_channel__cast_message__unpack(nullptr, message_length, buffer);

        if (message) {
            controller->handle_incoming_message(message);
            extensions__api__cast_channel__cast_message__free_unpacked(message, nullptr);
            consecutive_errors = 0; // Reset error counter on successful message
            message_count++;

            // Log memory status every 10 messages
            if (message_count % 10 == 0) {
                size_t free_heap = esp_get_free_heap_size();
                ESP_LOGD(TAG, "Processed %d messages, Free heap: %d bytes", message_count, free_heap);

                // Force garbage collection if memory is getting low
                if (free_heap < 16384) { // Less than 16KB
                    ESP_LOGW(TAG, "Low memory detected (%d bytes), forcing delay", free_heap);
                    vTaskDelay(pdMS_TO_TICKS(100)); // Longer delay to allow cleanup
                }
            }
        } else {
            ESP_LOGE(TAG, "Failed to unpack protobuf message");
            consecutive_errors++;
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent tight loop
    }

    // If we exit due to errors, update connection state
    if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
        ESP_LOGE(TAG, "Too many consecutive errors, marking connection as failed");
        controller->current_state = ChromecastController::ERROR_STATE;
        if (controller->state_callback) {
            controller->state_callback(controller->current_state);
        }
    }

    ESP_LOGI(TAG, "Receive task ended");

    // Clean up allocated buffer
    free(buffer);
    vTaskDelete(nullptr);
}

void ChromecastController::handle_incoming_message(const Extensions__Api__CastChannel__CastMessage* message) {
    if (!message->namespace_ || !message->payload_utf8) {
        ESP_LOGW(TAG, "Received message with missing namespace or payload");
        return;
    }

    // Use const char* directly to avoid string copying and reduce stack usage
    const char* namespace_str = message->namespace_;
    const char* payload = message->payload_utf8;
    size_t payload_len = strlen(payload);

    ESP_LOGI(TAG, "RECV <- Namespace: %s, Size: %d bytes", namespace_str, payload_len);
    ESP_LOGD(TAG, "RECV <- Payload: %s", payload);

    // Handle heartbeat messages
    if (strcmp(namespace_str, NAMESPACE_HEARTBEAT) == 0) {
        std::string payload_str(payload);
        cJSON* json = safe_json_parse(payload_str, 4096);
        if (json) {
            cJSON* type = cJSON_GetObjectItem(json, "type");
            if (type && cJSON_IsString(type)) {
                if (strcmp(type->valuestring, "PING") == 0) {
                    ESP_LOGD(TAG, "Received PING, responding with PONG");
                    std::string pong_msg = create_json_message("PONG");
                    bool success = send_protobuf_message(NAMESPACE_HEARTBEAT, pong_msg);
                    if (!success) {
                        ESP_LOGW(TAG, "Failed to send PONG response");
                    }
                } else if (strcmp(type->valuestring, "PONG") == 0) {
                    ESP_LOGD(TAG, "Received PONG - heartbeat acknowledged");
                }
            }
            cJSON_Delete(json);
        } else {
            ESP_LOGW(TAG, "Failed to parse heartbeat JSON: %s", payload);
        }
    }
    // Handle connection messages
    else if (namespace_str == NAMESPACE_CONNECTION) {
        cJSON* json = safe_json_parse(payload, 4096);
        if (json) {
            cJSON* type = cJSON_GetObjectItem(json, "type");
            if (type && cJSON_IsString(type)) {
                ESP_LOGI(TAG, "Connection message type: %s", type->valuestring);
                if (strcmp(type->valuestring, "CLOSE") == 0) {
                    ESP_LOGW(TAG, "Received CLOSE message from Chromecast");
                }
            }
            cJSON_Delete(json);
        }
    }
    // Handle receiver messages
    else if (namespace_str == NAMESPACE_RECEIVER) {
        process_receiver_message(payload);
    }
    else {
        ESP_LOGD(TAG, "Unhandled namespace: %s", namespace_str);
    }

    // Call user callback if set
    if (message_callback) {
        message_callback(namespace_str, payload);
    }
}

void ChromecastController::process_receiver_message(const std::string& payload) {
    cJSON* json = safe_json_parse(payload, 8192);
    if (!json) {
        return;
    }

    cJSON* type = cJSON_GetObjectItem(json, "type");
    if (type && cJSON_IsString(type)) {
        if (strcmp(type->valuestring, "RECEIVER_STATUS") == 0) {
            // Extract volume information
            cJSON* status = cJSON_GetObjectItem(json, "status");
            if (status) {
                cJSON* volume_obj = cJSON_GetObjectItem(status, "volume");
                if (volume_obj) {
                    VolumeInfo volume_info = {0.0f, false};

                    cJSON* level = cJSON_GetObjectItem(volume_obj, "level");
                    if (level && cJSON_IsNumber(level)) {
                        volume_info.level = level->valuedouble;
                    }

                    cJSON* muted = cJSON_GetObjectItem(volume_obj, "muted");
                    if (muted && cJSON_IsBool(muted)) {
                        volume_info.muted = cJSON_IsTrue(muted);
                    }

                    ESP_LOGI(TAG, "Volume status - Level: %.2f, Muted: %s",
                            volume_info.level, volume_info.muted ? "true" : "false");

                    if (volume_callback) {
                        volume_callback(volume_info);
                    }
                }
            }
        }
    }

    cJSON_Delete(json);
}

void ChromecastController::run_message_loop() {
    ESP_LOGI(TAG, "Starting message loop");

    while (is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(100)); // Main loop delay

        // You can add periodic tasks here if needed
        // For example, periodic status requests
    }

    ESP_LOGI(TAG, "Message loop ended");
}

cJSON* ChromecastController::safe_json_parse(const std::string& payload, size_t min_free_heap) {
    // Check available memory before parsing
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < min_free_heap) {
        ESP_LOGW(TAG, "Insufficient memory for JSON parsing: %d bytes available, %d required",
                 free_heap, min_free_heap);
        return nullptr;
    }

    // Check payload size is reasonable
    if (payload.length() > 4096) { // Limit to 4KB JSON messages
        ESP_LOGW(TAG, "JSON payload too large: %d bytes", payload.length());
        return nullptr;
    }

    // Attempt to parse with error handling
    cJSON* json = cJSON_Parse(payload.c_str());
    if (!json) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != nullptr) {
            ESP_LOGW(TAG, "JSON parse error near: %.20s", error_ptr);
        } else {
            ESP_LOGW(TAG, "JSON parse failed - likely memory allocation error");
        }

        // Log memory status after failure
        ESP_LOGW(TAG, "Free heap after parse failure: %d bytes", esp_get_free_heap_size());
    }

    return json;
}

void ChromecastController::log_memory_status(const char* context) {
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();

    if (context) {
        ESP_LOGI(TAG, "Memory status [%s]: Free: %d bytes, Min free: %d bytes",
                 context, free_heap, min_free_heap);
    } else {
        ESP_LOGI(TAG, "Memory status: Free: %d bytes, Min free: %d bytes",
                 free_heap, min_free_heap);
    }

    // Log warning if memory is getting low
    if (free_heap < 32768) { // Less than 32KB
        ESP_LOGW(TAG, "Low memory warning: Only %d bytes free", free_heap);
    }
}

bool ChromecastController::is_connection_healthy() const {
    // Check basic connection state
    if (current_state != CONNECTED) {
        return false;
    }

    // Check if TLS handle is valid
    if (!tls_handle) {
        return false;
    }

    // Check if virtual connection is established
    if (!virtual_connection_established) {
        return false;
    }

    return true;
}
