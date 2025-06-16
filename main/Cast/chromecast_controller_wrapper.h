#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief C wrapper for ChromecastController C++ class
 * 
 * This wrapper provides a C interface to the ChromecastController C++ class,
 * allowing C code to use the Chromecast control functionality.
 */

// Opaque handle for ChromecastController instance
typedef void* chromecast_controller_handle_t;

// Connection states
typedef enum {
    CHROMECAST_DISCONNECTED,
    CHROMECAST_CONNECTING,
    CHROMECAST_CONNECTED,
    CHROMECAST_ERROR_STATE
} chromecast_connection_state_t;

// Volume information structure (C compatible)
typedef struct {
    float level;     // 0.0 to 1.0
    bool muted;
} chromecast_volume_info_t;

// Callback function types
typedef void (*chromecast_state_callback_t)(chromecast_connection_state_t state);
typedef void (*chromecast_volume_callback_t)(const chromecast_volume_info_t* volume);
typedef void (*chromecast_message_callback_t)(const char* namespace_str, const char* payload);

/**
 * @brief Create a new ChromecastController instance
 * 
 * @return chromecast_controller_handle_t Handle to the controller instance, NULL on failure
 */
chromecast_controller_handle_t chromecast_controller_create(void);

/**
 * @brief Destroy a ChromecastController instance
 * 
 * @param handle Controller instance handle
 */
void chromecast_controller_destroy(chromecast_controller_handle_t handle);

/**
 * @brief Initialize the ChromecastController
 * 
 * @param handle Controller instance handle
 * @return bool true on success, false on failure
 */
bool chromecast_controller_initialize(chromecast_controller_handle_t handle);

/**
 * @brief Connect to a Chromecast device
 * 
 * @param handle Controller instance handle
 * @param ip IP address of the Chromecast device
 * @return bool true on success, false on failure
 */
bool chromecast_controller_connect(chromecast_controller_handle_t handle, const char* ip);

/**
 * @brief Disconnect from the Chromecast device
 * 
 * @param handle Controller instance handle
 */
void chromecast_controller_disconnect(chromecast_controller_handle_t handle);

/**
 * @brief Set volume level and mute state
 * 
 * @param handle Controller instance handle
 * @param level Volume level (0.0 to 1.0)
 * @param muted Mute state
 * @return bool true on success, false on failure
 */
bool chromecast_controller_set_volume(chromecast_controller_handle_t handle, float level, bool muted);

/**
 * @brief Get current status from the Chromecast device
 * 
 * @param handle Controller instance handle
 * @return bool true on success, false on failure
 */
bool chromecast_controller_get_status(chromecast_controller_handle_t handle);

/**
 * @brief Get current connection state
 * 
 * @param handle Controller instance handle
 * @return chromecast_connection_state_t Current connection state
 */
chromecast_connection_state_t chromecast_controller_get_state(chromecast_controller_handle_t handle);

/**
 * @brief Get connected device IP address
 * 
 * @param handle Controller instance handle
 * @param ip_buffer Buffer to store IP address string
 * @param buffer_size Size of the buffer
 * @return bool true if connected and IP copied, false otherwise
 */
bool chromecast_controller_get_connected_device(chromecast_controller_handle_t handle, 
                                               char* ip_buffer, size_t buffer_size);

/**
 * @brief Set state change callback
 * 
 * @param handle Controller instance handle
 * @param callback Callback function for state changes
 */
void chromecast_controller_set_state_callback(chromecast_controller_handle_t handle, 
                                             chromecast_state_callback_t callback);

/**
 * @brief Set volume change callback
 * 
 * @param handle Controller instance handle
 * @param callback Callback function for volume changes
 */
void chromecast_controller_set_volume_callback(chromecast_controller_handle_t handle, 
                                              chromecast_volume_callback_t callback);

/**
 * @brief Set message callback
 * 
 * @param handle Controller instance handle
 * @param callback Callback function for received messages
 */
void chromecast_controller_set_message_callback(chromecast_controller_handle_t handle, 
                                               chromecast_message_callback_t callback);

/**
 * @brief Start heartbeat timer
 * 
 * @param handle Controller instance handle
 */
void chromecast_controller_start_heartbeat(chromecast_controller_handle_t handle);

/**
 * @brief Stop heartbeat timer
 * 
 * @param handle Controller instance handle
 */
void chromecast_controller_stop_heartbeat(chromecast_controller_handle_t handle);

#ifdef __cplusplus
}
#endif
