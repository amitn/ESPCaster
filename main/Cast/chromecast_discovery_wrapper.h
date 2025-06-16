#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief C wrapper for ChromecastDiscovery C++ class
 * 
 * This wrapper provides a C interface to the ChromecastDiscovery C++ class,
 * allowing C code to use the Chromecast discovery functionality.
 */

// Opaque handle for ChromecastDiscovery instance
typedef void* chromecast_discovery_handle_t;

// Device information structure (C compatible)
typedef struct {
    char name[64];           // Device friendly name
    char ip_address[16];     // IP address as string
    int port;                // Port number (usually 8009)
    char instance_name[64];  // mDNS instance name
    char model[32];          // Device model (if available)
    char uuid[64];           // Device UUID (if available)
} chromecast_device_info_t;

// Discovery result callback
typedef void (*chromecast_discovery_callback_t)(const chromecast_device_info_t* devices, size_t device_count);
typedef void (*chromecast_device_found_callback_t)(const chromecast_device_info_t* device);

/**
 * @brief Create a new ChromecastDiscovery instance
 * 
 * @return chromecast_discovery_handle_t Handle to the discovery instance, NULL on failure
 */
chromecast_discovery_handle_t chromecast_discovery_create(void);

/**
 * @brief Destroy a ChromecastDiscovery instance
 * 
 * @param handle Discovery instance handle
 */
void chromecast_discovery_destroy(chromecast_discovery_handle_t handle);

/**
 * @brief Initialize the discovery instance
 * 
 * @param handle Discovery instance handle
 * @return bool true on success, false on failure
 */
bool chromecast_discovery_initialize(chromecast_discovery_handle_t handle);

/**
 * @brief Deinitialize the discovery instance
 * 
 * @param handle Discovery instance handle
 */
void chromecast_discovery_deinitialize(chromecast_discovery_handle_t handle);

/**
 * @brief Discover Chromecast devices synchronously
 * 
 * @param handle Discovery instance handle
 * @param devices Array to store discovered devices
 * @param max_devices Maximum number of devices to store
 * @param device_count Pointer to store actual number of devices found
 * @return bool true on success, false on failure
 */
bool chromecast_discovery_discover_sync(chromecast_discovery_handle_t handle,
                                       chromecast_device_info_t* devices,
                                       size_t max_devices,
                                       size_t* device_count);

/**
 * @brief Start asynchronous discovery
 * 
 * @param handle Discovery instance handle
 * @return bool true on success, false on failure
 */
bool chromecast_discovery_discover_async(chromecast_discovery_handle_t handle);

/**
 * @brief Start periodic discovery
 * 
 * @param handle Discovery instance handle
 * @param interval_ms Discovery interval in milliseconds
 * @return bool true on success, false on failure
 */
bool chromecast_discovery_start_periodic(chromecast_discovery_handle_t handle, uint32_t interval_ms);

/**
 * @brief Stop periodic discovery
 * 
 * @param handle Discovery instance handle
 */
void chromecast_discovery_stop_periodic(chromecast_discovery_handle_t handle);

/**
 * @brief Set discovery timeout
 * 
 * @param handle Discovery instance handle
 * @param timeout_ms Timeout in milliseconds
 */
void chromecast_discovery_set_timeout(chromecast_discovery_handle_t handle, uint32_t timeout_ms);

/**
 * @brief Set maximum number of results
 * 
 * @param handle Discovery instance handle
 * @param max_results Maximum number of results
 */
void chromecast_discovery_set_max_results(chromecast_discovery_handle_t handle, size_t max_results);

/**
 * @brief Set discovery callback
 * 
 * @param handle Discovery instance handle
 * @param callback Callback function to call when discovery completes
 */
void chromecast_discovery_set_callback(chromecast_discovery_handle_t handle, 
                                      chromecast_discovery_callback_t callback);

/**
 * @brief Set device found callback
 * 
 * @param handle Discovery instance handle
 * @param callback Callback function to call when a device is found
 */
void chromecast_discovery_set_device_found_callback(chromecast_discovery_handle_t handle,
                                                   chromecast_device_found_callback_t callback);

/**
 * @brief Check if discovery is initialized
 * 
 * @param handle Discovery instance handle
 * @return bool true if initialized, false otherwise
 */
bool chromecast_discovery_is_initialized(chromecast_discovery_handle_t handle);

/**
 * @brief Check if discovery is active
 * 
 * @param handle Discovery instance handle
 * @return bool true if active, false otherwise
 */
bool chromecast_discovery_is_active(chromecast_discovery_handle_t handle);

/**
 * @brief Convert device info to string
 * 
 * @param device Device information
 * @param buffer Buffer to store the string
 * @param buffer_size Size of the buffer
 * @return bool true on success, false on failure
 */
bool chromecast_device_info_to_string(const chromecast_device_info_t* device, char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif
