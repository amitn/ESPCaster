#include "spotify_config_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "spotify_config";

// Default redirect URI for local callback server
static const char* DEFAULT_REDIRECT_URI = "http://localhost:8888/callback";

// Internal state
static bool g_initialized = false;

esp_err_t spotify_config_manager_init(void) {
    if (g_initialized) {
        ESP_LOGW(TAG, "Spotify config manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing Spotify configuration manager");

    // Initialize NVS if not already done
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    g_initialized = true;
    ESP_LOGI(TAG, "Spotify configuration manager initialized successfully");
    return ESP_OK;
}

esp_err_t spotify_config_manager_deinit(void) {
    if (!g_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing Spotify configuration manager");
    g_initialized = false;
    return ESP_OK;
}

esp_err_t spotify_config_save(const spotify_config_t *config) {
    if (!g_initialized) {
        ESP_LOGE(TAG, "Config manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!config) {
        ESP_LOGE(TAG, "Invalid config parameter");
        return ESP_ERR_INVALID_ARG;
    }

    if (!spotify_config_validate(config)) {
        ESP_LOGE(TAG, "Invalid configuration provided");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(SPOTIFY_CONFIG_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    bool success = true;

    // Save client ID
    if (nvs_set_str(nvs_handle, SPOTIFY_CONFIG_CLIENT_ID_KEY, config->client_id) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save client ID");
        success = false;
    }

    // Save client secret (optional)
    if (success && strlen(config->client_secret) > 0) {
        if (nvs_set_str(nvs_handle, SPOTIFY_CONFIG_CLIENT_SECRET_KEY, config->client_secret) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save client secret");
            success = false;
        }
    }

    // Save redirect URI
    if (success) {
        const char* uri_to_save = strlen(config->redirect_uri) > 0 ? 
                                  config->redirect_uri : DEFAULT_REDIRECT_URI;
        if (nvs_set_str(nvs_handle, SPOTIFY_CONFIG_REDIRECT_URI_KEY, uri_to_save) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save redirect URI");
            success = false;
        }
    }

    if (success) {
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
            success = false;
        } else {
            ESP_LOGI(TAG, "Successfully saved Spotify configuration");
        }
    }

    nvs_close(nvs_handle);
    return success ? ESP_OK : ESP_FAIL;
}

esp_err_t spotify_config_load(spotify_config_t *config) {
    if (!g_initialized) {
        ESP_LOGE(TAG, "Config manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!config) {
        ESP_LOGE(TAG, "Invalid config parameter");
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize config structure
    memset(config, 0, sizeof(spotify_config_t));

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(SPOTIFY_CONFIG_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No Spotify configuration found (NVS namespace not found)");
        return ESP_ERR_NOT_FOUND;
    }

    size_t required_size;
    bool success = true;

    // Load client ID
    required_size = SPOTIFY_CLIENT_ID_MAX_LEN;
    err = nvs_get_str(nvs_handle, SPOTIFY_CONFIG_CLIENT_ID_KEY, config->client_id, &required_size);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No client ID found in configuration");
        success = false;
    }

    // Load client secret (optional)
    if (success) {
        required_size = SPOTIFY_CLIENT_SECRET_MAX_LEN;
        err = nvs_get_str(nvs_handle, SPOTIFY_CONFIG_CLIENT_SECRET_KEY, config->client_secret, &required_size);
        if (err != ESP_OK) {
            ESP_LOGD(TAG, "No client secret found (using PKCE flow)");
            // Client secret is optional, so this is not an error
            config->client_secret[0] = '\0';
        }
    }

    // Load redirect URI
    if (success) {
        required_size = SPOTIFY_REDIRECT_URI_MAX_LEN;
        err = nvs_get_str(nvs_handle, SPOTIFY_CONFIG_REDIRECT_URI_KEY, config->redirect_uri, &required_size);
        if (err != ESP_OK) {
            ESP_LOGD(TAG, "No redirect URI found, using default");
            strncpy(config->redirect_uri, DEFAULT_REDIRECT_URI, SPOTIFY_REDIRECT_URI_MAX_LEN - 1);
            config->redirect_uri[SPOTIFY_REDIRECT_URI_MAX_LEN - 1] = '\0';
        }
    }

    nvs_close(nvs_handle);

    if (success) {
        config->is_configured = true;
        ESP_LOGI(TAG, "Successfully loaded Spotify configuration");
        return ESP_OK;
    } else {
        config->is_configured = false;
        return ESP_ERR_NOT_FOUND;
    }
}

bool spotify_config_is_configured(void) {
    spotify_config_t config;
    esp_err_t ret = spotify_config_load(&config);
    return (ret == ESP_OK && config.is_configured);
}

esp_err_t spotify_config_clear(void) {
    if (!g_initialized) {
        ESP_LOGE(TAG, "Config manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(SPOTIFY_CONFIG_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle for clearing: %s", esp_err_to_name(err));
        return err;
    }

    nvs_erase_key(nvs_handle, SPOTIFY_CONFIG_CLIENT_ID_KEY);
    nvs_erase_key(nvs_handle, SPOTIFY_CONFIG_CLIENT_SECRET_KEY);
    nvs_erase_key(nvs_handle, SPOTIFY_CONFIG_REDIRECT_URI_KEY);

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Cleared Spotify configuration from NVS");
    } else {
        ESP_LOGE(TAG, "Failed to clear Spotify configuration: %s", esp_err_to_name(err));
    }

    return err;
}

bool spotify_config_validate(const spotify_config_t *config) {
    if (!config) {
        return false;
    }

    // Client ID is required and must be non-empty
    if (strlen(config->client_id) == 0) {
        ESP_LOGE(TAG, "Client ID is required");
        return false;
    }

    // Client ID should be reasonable length (Spotify client IDs are typically 32 chars)
    if (strlen(config->client_id) < 10 || strlen(config->client_id) > 100) {
        ESP_LOGE(TAG, "Client ID length is invalid");
        return false;
    }

    // Client secret is optional (for PKCE flow)
    if (strlen(config->client_secret) > 0) {
        if (strlen(config->client_secret) < 10 || strlen(config->client_secret) > 100) {
            ESP_LOGE(TAG, "Client secret length is invalid");
            return false;
        }
    }

    // Redirect URI validation (basic check)
    const char* uri = strlen(config->redirect_uri) > 0 ? config->redirect_uri : DEFAULT_REDIRECT_URI;
    if (strncmp(uri, "http://", 7) != 0 && strncmp(uri, "https://", 8) != 0) {
        ESP_LOGE(TAG, "Redirect URI must start with http:// or https://");
        return false;
    }

    return true;
}

bool spotify_config_validate_detailed(const spotify_config_t *config, char *error_msg, size_t error_msg_len) {
    if (!config) {
        if (error_msg && error_msg_len > 0) {
            strncpy(error_msg, "Configuration is NULL", error_msg_len - 1);
            error_msg[error_msg_len - 1] = '\0';
        }
        return false;
    }

    // Client ID is required and must be non-empty
    if (strlen(config->client_id) == 0) {
        if (error_msg && error_msg_len > 0) {
            strncpy(error_msg, "Client ID is required", error_msg_len - 1);
            error_msg[error_msg_len - 1] = '\0';
        }
        return false;
    }

    // Client ID should be reasonable length (Spotify client IDs are typically 32 chars)
    if (strlen(config->client_id) < 10) {
        if (error_msg && error_msg_len > 0) {
            strncpy(error_msg, "Client ID is too short (minimum 10 characters)", error_msg_len - 1);
            error_msg[error_msg_len - 1] = '\0';
        }
        return false;
    }

    if (strlen(config->client_id) > 100) {
        if (error_msg && error_msg_len > 0) {
            strncpy(error_msg, "Client ID is too long (maximum 100 characters)", error_msg_len - 1);
            error_msg[error_msg_len - 1] = '\0';
        }
        return false;
    }

    // Client secret is optional (for PKCE flow)
    if (strlen(config->client_secret) > 0) {
        if (strlen(config->client_secret) < 10) {
            if (error_msg && error_msg_len > 0) {
                strncpy(error_msg, "Client Secret is too short (minimum 10 characters)", error_msg_len - 1);
                error_msg[error_msg_len - 1] = '\0';
            }
            return false;
        }
        if (strlen(config->client_secret) > 100) {
            if (error_msg && error_msg_len > 0) {
                strncpy(error_msg, "Client Secret is too long (maximum 100 characters)", error_msg_len - 1);
                error_msg[error_msg_len - 1] = '\0';
            }
            return false;
        }
    }

    // Redirect URI validation (basic check)
    const char* uri = strlen(config->redirect_uri) > 0 ? config->redirect_uri : DEFAULT_REDIRECT_URI;
    if (strncmp(uri, "http://", 7) != 0 && strncmp(uri, "https://", 8) != 0) {
        if (error_msg && error_msg_len > 0) {
            strncpy(error_msg, "Redirect URI must start with http:// or https://", error_msg_len - 1);
            error_msg[error_msg_len - 1] = '\0';
        }
        return false;
    }

    return true;
}

const char* spotify_config_get_default_redirect_uri(void) {
    return DEFAULT_REDIRECT_URI;
}
