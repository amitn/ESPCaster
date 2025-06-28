#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Spotify Configuration Manager - Handles credential storage and retrieval
 * 
 * This component manages Spotify app credentials (Client ID, Client Secret, Redirect URI)
 * using NVS (Non-Volatile Storage) for persistent storage across reboots.
 */

// Maximum lengths for Spotify credentials
#define SPOTIFY_CLIENT_ID_MAX_LEN       128
#define SPOTIFY_CLIENT_SECRET_MAX_LEN   128
#define SPOTIFY_REDIRECT_URI_MAX_LEN    256

// NVS storage keys
#define SPOTIFY_CONFIG_NAMESPACE        "spotify_cfg"
#define SPOTIFY_CONFIG_CLIENT_ID_KEY    "client_id"
#define SPOTIFY_CONFIG_CLIENT_SECRET_KEY "client_secret"
#define SPOTIFY_CONFIG_REDIRECT_URI_KEY "redirect_uri"

/**
 * @brief Spotify configuration structure
 */
typedef struct {
    char client_id[SPOTIFY_CLIENT_ID_MAX_LEN];
    char client_secret[SPOTIFY_CLIENT_SECRET_MAX_LEN];
    char redirect_uri[SPOTIFY_REDIRECT_URI_MAX_LEN];
    bool is_configured;
} spotify_config_t;

/**
 * @brief Initialize Spotify configuration manager
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t spotify_config_manager_init(void);

/**
 * @brief Deinitialize Spotify configuration manager
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t spotify_config_manager_deinit(void);

/**
 * @brief Save Spotify configuration to NVS
 * 
 * @param config Configuration to save
 * @return esp_err_t ESP_OK on success
 */
esp_err_t spotify_config_save(const spotify_config_t *config);

/**
 * @brief Load Spotify configuration from NVS
 * 
 * @param config Configuration structure to fill
 * @return esp_err_t ESP_OK on success, ESP_ERR_NOT_FOUND if no config exists
 */
esp_err_t spotify_config_load(spotify_config_t *config);

/**
 * @brief Check if Spotify is configured
 * 
 * @return bool true if valid configuration exists
 */
bool spotify_config_is_configured(void);

/**
 * @brief Clear stored Spotify configuration
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t spotify_config_clear(void);

/**
 * @brief Validate Spotify configuration
 *
 * @param config Configuration to validate
 * @return bool true if configuration is valid
 */
bool spotify_config_validate(const spotify_config_t *config);

/**
 * @brief Validate Spotify configuration with detailed error message
 *
 * @param config Configuration to validate
 * @param error_msg Buffer to store error message (can be NULL)
 * @param error_msg_len Length of error message buffer
 * @return bool true if configuration is valid
 */
bool spotify_config_validate_detailed(const spotify_config_t *config, char *error_msg, size_t error_msg_len);

/**
 * @brief Get default redirect URI
 * 
 * @return const char* Default redirect URI
 */
const char* spotify_config_get_default_redirect_uri(void);

#ifdef __cplusplus
}
#endif
