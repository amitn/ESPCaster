#include "spotify_auth.h"
#include "spotify_controller.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <cstring>
#include <sstream>
#include <iomanip>

static const char *TAG = "spotify_auth";

// NVS storage keys
static const char* NVS_NAMESPACE = "spotify_auth";
static const char* NVS_ACCESS_TOKEN_KEY = "access_token";
static const char* NVS_REFRESH_TOKEN_KEY = "refresh_token";
static const char* NVS_TOKEN_TYPE_KEY = "token_type";
static const char* NVS_EXPIRES_AT_KEY = "expires_at";
static const char* NVS_SCOPE_KEY = "scope";

SpotifyAuth::SpotifyAuth() 
    : auth_state(SpotifyAuthState::NOT_AUTHENTICATED)
    , callback_server(nullptr)
    , server_running(false)
    , auth_state_callback(nullptr)
    , token_callback(nullptr)
    , error_callback(nullptr)
    , callback_user_data(nullptr) {
    
    // Initialize token structure
    memset(&current_tokens, 0, sizeof(current_tokens));
}

SpotifyAuth::~SpotifyAuth() {
    deinitialize();
}

bool SpotifyAuth::initialize(const std::string& client_id, 
                           const std::string& redirect_uri,
                           const std::string& scope) {
    ESP_LOGI(TAG, "Initializing Spotify authentication");
    
    this->client_id = client_id;
    this->redirect_uri = redirect_uri;
    this->scope = scope;
    
    // Generate PKCE parameters
    code_verifier = generate_code_verifier();
    code_challenge = generate_code_challenge(code_verifier);
    state = generate_random_string(16);
    
    ESP_LOGI(TAG, "Generated PKCE parameters");
    ESP_LOGD(TAG, "Code verifier length: %d", code_verifier.length());
    ESP_LOGD(TAG, "Code challenge: %s", code_challenge.c_str());
    
    // Try to load existing tokens
    if (load_tokens_from_nvs()) {
        ESP_LOGI(TAG, "Loaded existing tokens from NVS");
        if (is_token_valid()) {
            update_auth_state(SpotifyAuthState::AUTHENTICATED);
        } else {
            ESP_LOGI(TAG, "Existing tokens are expired, will need to refresh");
            update_auth_state(SpotifyAuthState::TOKEN_EXPIRED);
        }
    } else {
        ESP_LOGI(TAG, "No existing tokens found");
        update_auth_state(SpotifyAuthState::NOT_AUTHENTICATED);
    }
    
    return true;
}

void SpotifyAuth::deinitialize() {
    ESP_LOGI(TAG, "Deinitializing Spotify authentication");
    
    stop_callback_server();
    
    // Clear sensitive data
    code_verifier.clear();
    code_challenge.clear();
    state.clear();
    
    update_auth_state(SpotifyAuthState::NOT_AUTHENTICATED);
}

std::string SpotifyAuth::generate_random_string(size_t length) {
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        uint32_t random_val = esp_random();
        result += charset[random_val % (sizeof(charset) - 1)];
    }
    
    return result;
}

std::string SpotifyAuth::generate_code_verifier() {
    // PKCE code verifier: 43-128 characters, URL-safe
    return generate_random_string(128);
}

std::string SpotifyAuth::generate_code_challenge(const std::string& verifier) {
    // SHA256 hash of code verifier, then base64url encode
    unsigned char hash[32];
    mbedtls_sha256_context ctx;
    
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0); // 0 = SHA256, not SHA224
    mbedtls_sha256_update(&ctx, (const unsigned char*)verifier.c_str(), verifier.length());
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    
    // Base64url encode (without padding)
    size_t olen;
    unsigned char base64_buf[64];
    
    int ret = mbedtls_base64_encode(base64_buf, sizeof(base64_buf), &olen, hash, 32);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to base64 encode code challenge");
        return "";
    }
    
    std::string result((char*)base64_buf, olen);
    
    // Convert to base64url (replace + with -, / with _, remove padding)
    for (char& c : result) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    
    // Remove padding
    size_t padding_pos = result.find('=');
    if (padding_pos != std::string::npos) {
        result = result.substr(0, padding_pos);
    }
    
    return result;
}

std::string SpotifyAuth::url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    
    for (char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int((unsigned char)c);
            escaped << std::nouppercase;
        }
    }
    
    return escaped.str();
}

std::string SpotifyAuth::get_authorization_url() {
    if (client_id.empty()) {
        ESP_LOGE(TAG, "Client ID not set");
        return "";
    }
    
    std::ostringstream url;
    url << SPOTIFY_AUTH_URL << "?"
        << "response_type=code"
        << "&client_id=" << url_encode(client_id)
        << "&scope=" << url_encode(scope)
        << "&redirect_uri=" << url_encode(redirect_uri)
        << "&state=" << url_encode(state)
        << "&code_challenge_method=S256"
        << "&code_challenge=" << url_encode(code_challenge);
    
    ESP_LOGI(TAG, "Generated authorization URL");
    return url.str();
}

bool SpotifyAuth::start_callback_server() {
    if (server_running) {
        ESP_LOGW(TAG, "Callback server already running");
        return true;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CALLBACK_SERVER_PORT;
    config.max_uri_handlers = 2;
    
    esp_err_t ret = httpd_start(&callback_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start callback server: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Register callback handler
    httpd_uri_t callback_uri = {
        .uri = "/callback",
        .method = HTTP_GET,
        .handler = callback_handler,
        .user_ctx = this
    };
    httpd_register_uri_handler(callback_server, &callback_uri);
    
    // Register root handler
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = this
    };
    httpd_register_uri_handler(callback_server, &root_uri);
    
    server_running = true;
    ESP_LOGI(TAG, "Callback server started on port %d", CALLBACK_SERVER_PORT);
    return true;
}

void SpotifyAuth::stop_callback_server() {
    if (callback_server && server_running) {
        httpd_stop(callback_server);
        callback_server = nullptr;
        server_running = false;
        ESP_LOGI(TAG, "Callback server stopped");
    }
}

esp_err_t SpotifyAuth::callback_handler(httpd_req_t* req) {
    SpotifyAuth* auth = static_cast<SpotifyAuth*>(req->user_ctx);
    
    // Parse query parameters
    char query[512];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char code[256] = {0};
        char state[64] = {0};
        char error[256] = {0};
        
        // Extract parameters
        httpd_query_key_value(query, "code", code, sizeof(code));
        httpd_query_key_value(query, "state", state, sizeof(state));
        httpd_query_key_value(query, "error", error, sizeof(error));
        
        ESP_LOGI(TAG, "Received callback with code: %s, state: %s", 
                 code[0] ? "present" : "missing", 
                 state[0] ? state : "missing");
        
        if (error[0]) {
            ESP_LOGE(TAG, "Authorization error: %s", error);
            auth->update_auth_state(SpotifyAuthState::ERROR_STATE);
            
            const char* error_response = 
                "<!DOCTYPE html><html><body>"
                "<h1>Authorization Failed</h1>"
                "<p>There was an error during authorization. Please try again.</p>"
                "</body></html>";
            httpd_resp_send(req, error_response, strlen(error_response));
            return ESP_OK;
        }
        
        if (code[0] && state[0]) {
            // Verify state parameter
            if (auth->state == state) {
                // Exchange code for tokens
                if (auth->exchange_code_for_tokens(code)) {
                    const char* success_response = 
                        "<!DOCTYPE html><html><body>"
                        "<h1>Authorization Successful</h1>"
                        "<p>You can now close this window and return to the app.</p>"
                        "</body></html>";
                    httpd_resp_send(req, success_response, strlen(success_response));
                } else {
                    const char* error_response = 
                        "<!DOCTYPE html><html><body>"
                        "<h1>Token Exchange Failed</h1>"
                        "<p>Failed to exchange authorization code for tokens.</p>"
                        "</body></html>";
                    httpd_resp_send(req, error_response, strlen(error_response));
                }
            } else {
                ESP_LOGE(TAG, "State parameter mismatch");
                auth->update_auth_state(SpotifyAuthState::ERROR_STATE);
                
                const char* error_response = 
                    "<!DOCTYPE html><html><body>"
                    "<h1>Security Error</h1>"
                    "<p>State parameter mismatch. Please try again.</p>"
                    "</body></html>";
                httpd_resp_send(req, error_response, strlen(error_response));
            }
        } else {
            ESP_LOGE(TAG, "Missing required parameters in callback");
            const char* error_response = 
                "<!DOCTYPE html><html><body>"
                "<h1>Invalid Request</h1>"
                "<p>Missing required parameters.</p>"
                "</body></html>";
            httpd_resp_send(req, error_response, strlen(error_response));
        }
    } else {
        ESP_LOGE(TAG, "Failed to parse query string");
        const char* error_response = 
            "<!DOCTYPE html><html><body>"
            "<h1>Invalid Request</h1>"
            "<p>Failed to parse request parameters.</p>"
            "</body></html>";
        httpd_resp_send(req, error_response, strlen(error_response));
    }
    
    return ESP_OK;
}

esp_err_t SpotifyAuth::root_handler(httpd_req_t* req) {
    const char* response = 
        "<!DOCTYPE html><html><body>"
        "<h1>Spotify Authentication</h1>"
        "<p>This is the callback server for Spotify authentication.</p>"
        "<p>Please use the proper authorization URL to authenticate.</p>"
        "</body></html>";
    
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

bool SpotifyAuth::exchange_code_for_tokens(const std::string& auth_code) {
    ESP_LOGI(TAG, "Exchanging authorization code for tokens");

    update_auth_state(SpotifyAuthState::AUTHENTICATING);

    // Prepare HTTP client
    esp_http_client_config_t config = {};
    config.url = SPOTIFY_TOKEN_URL;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 10000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        update_auth_state(SpotifyAuthState::ERROR_STATE);
        return false;
    }

    // Prepare POST data
    std::ostringstream post_data;
    post_data << "grant_type=authorization_code"
              << "&code=" << url_encode(auth_code)
              << "&redirect_uri=" << url_encode(redirect_uri)
              << "&client_id=" << url_encode(client_id)
              << "&code_verifier=" << url_encode(code_verifier);

    std::string post_data_str = post_data.str();

    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, post_data_str.c_str(), post_data_str.length());

    // Perform request
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        update_auth_state(SpotifyAuthState::ERROR_STATE);
        return false;
    }

    // Check response status
    int status_code = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);

    ESP_LOGI(TAG, "Token exchange response: status=%d, length=%d", status_code, content_length);

    if (status_code != 200) {
        ESP_LOGE(TAG, "Token exchange failed with status: %d", status_code);
        esp_http_client_cleanup(client);
        update_auth_state(SpotifyAuthState::ERROR_STATE);
        return false;
    }

    // Read response
    char* response_buffer = (char*)malloc(content_length + 1);
    if (!response_buffer) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        esp_http_client_cleanup(client);
        update_auth_state(SpotifyAuthState::ERROR_STATE);
        return false;
    }

    int data_read = esp_http_client_read_response(client, response_buffer, content_length);
    response_buffer[data_read] = '\0';

    esp_http_client_cleanup(client);

    // Parse JSON response
    cJSON* json = cJSON_Parse(response_buffer);
    free(response_buffer);

    if (!json) {
        ESP_LOGE(TAG, "Failed to parse token response JSON");
        update_auth_state(SpotifyAuthState::ERROR_STATE);
        return false;
    }

    // Extract token information
    cJSON* access_token = cJSON_GetObjectItem(json, "access_token");
    cJSON* refresh_token = cJSON_GetObjectItem(json, "refresh_token");
    cJSON* token_type = cJSON_GetObjectItem(json, "token_type");
    cJSON* expires_in = cJSON_GetObjectItem(json, "expires_in");
    cJSON* scope = cJSON_GetObjectItem(json, "scope");

    if (!access_token || !cJSON_IsString(access_token)) {
        ESP_LOGE(TAG, "Invalid access token in response");
        cJSON_Delete(json);
        update_auth_state(SpotifyAuthState::ERROR_STATE);
        return false;
    }

    // Store tokens
    current_tokens.access_token = cJSON_GetStringValue(access_token);
    current_tokens.refresh_token = refresh_token && cJSON_IsString(refresh_token) ?
                                  cJSON_GetStringValue(refresh_token) : "";
    current_tokens.token_type = token_type && cJSON_IsString(token_type) ?
                               cJSON_GetStringValue(token_type) : "Bearer";
    current_tokens.expires_in = expires_in && cJSON_IsNumber(expires_in) ?
                               cJSON_GetNumberValue(expires_in) : 3600;
    current_tokens.scope = scope && cJSON_IsString(scope) ?
                          cJSON_GetStringValue(scope) : "";

    // Calculate expiration time
    time_t now;
    time(&now);
    current_tokens.expires_at = now + current_tokens.expires_in;

    cJSON_Delete(json);

    ESP_LOGI(TAG, "Successfully obtained tokens, expires in %d seconds", current_tokens.expires_in);

    // Save tokens to NVS
    save_tokens_to_nvs();

    update_auth_state(SpotifyAuthState::AUTHENTICATED);

    // Notify callback
    if (token_callback) {
        token_callback(current_tokens, callback_user_data);
    }

    return true;
}

bool SpotifyAuth::refresh_access_token() {
    if (current_tokens.refresh_token.empty()) {
        ESP_LOGE(TAG, "No refresh token available");
        return false;
    }

    ESP_LOGI(TAG, "Refreshing access token");

    // Prepare HTTP client
    esp_http_client_config_t config = {};
    config.url = SPOTIFY_TOKEN_URL;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 10000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for token refresh");
        return false;
    }

    // Prepare POST data
    std::ostringstream post_data;
    post_data << "grant_type=refresh_token"
              << "&refresh_token=" << url_encode(current_tokens.refresh_token)
              << "&client_id=" << url_encode(client_id);

    std::string post_data_str = post_data.str();

    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, post_data_str.c_str(), post_data_str.length());

    // Perform request
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Token refresh request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    // Check response status
    int status_code = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);

    if (status_code != 200) {
        ESP_LOGE(TAG, "Token refresh failed with status: %d", status_code);
        esp_http_client_cleanup(client);
        update_auth_state(SpotifyAuthState::ERROR_STATE);
        return false;
    }

    // Read response
    char* response_buffer = (char*)malloc(content_length + 1);
    if (!response_buffer) {
        ESP_LOGE(TAG, "Failed to allocate response buffer for token refresh");
        esp_http_client_cleanup(client);
        return false;
    }

    int data_read = esp_http_client_read_response(client, response_buffer, content_length);
    response_buffer[data_read] = '\0';

    esp_http_client_cleanup(client);

    // Parse JSON response
    cJSON* json = cJSON_Parse(response_buffer);
    free(response_buffer);

    if (!json) {
        ESP_LOGE(TAG, "Failed to parse token refresh response JSON");
        return false;
    }

    // Extract new token information
    cJSON* access_token = cJSON_GetObjectItem(json, "access_token");
    cJSON* refresh_token = cJSON_GetObjectItem(json, "refresh_token");
    cJSON* expires_in = cJSON_GetObjectItem(json, "expires_in");

    if (!access_token || !cJSON_IsString(access_token)) {
        ESP_LOGE(TAG, "Invalid access token in refresh response");
        cJSON_Delete(json);
        return false;
    }

    // Update tokens
    current_tokens.access_token = cJSON_GetStringValue(access_token);
    if (refresh_token && cJSON_IsString(refresh_token)) {
        current_tokens.refresh_token = cJSON_GetStringValue(refresh_token);
    }
    current_tokens.expires_in = expires_in && cJSON_IsNumber(expires_in) ?
                               cJSON_GetNumberValue(expires_in) : 3600;

    // Calculate new expiration time
    time_t now;
    time(&now);
    current_tokens.expires_at = now + current_tokens.expires_in;

    cJSON_Delete(json);

    ESP_LOGI(TAG, "Successfully refreshed access token");

    // Save updated tokens to NVS
    save_tokens_to_nvs();

    update_auth_state(SpotifyAuthState::AUTHENTICATED);

    return true;
}

void SpotifyAuth::update_auth_state(SpotifyAuthState new_state) {
    if (auth_state != new_state) {
        auth_state = new_state;
        ESP_LOGI(TAG, "Authentication state changed to: %d", (int)new_state);

        if (auth_state_callback) {
            auth_state_callback(new_state, callback_user_data);
        }
    }
}

bool SpotifyAuth::handle_authorization_response(const std::string& auth_code, const std::string& received_state) {
    if (received_state != state) {
        ESP_LOGE(TAG, "State parameter mismatch");
        update_auth_state(SpotifyAuthState::ERROR_STATE);
        return false;
    }

    return exchange_code_for_tokens(auth_code);
}

bool SpotifyAuth::refresh_token() {
    return refresh_access_token();
}

void SpotifyAuth::logout() {
    ESP_LOGI(TAG, "Logging out");

    // Clear tokens
    memset(&current_tokens, 0, sizeof(current_tokens));

    // Clear stored tokens
    clear_stored_tokens();

    update_auth_state(SpotifyAuthState::NOT_AUTHENTICATED);
}

bool SpotifyAuth::is_authenticated() const {
    return auth_state == SpotifyAuthState::AUTHENTICATED && !current_tokens.access_token.empty();
}

bool SpotifyAuth::is_token_valid() const {
    if (current_tokens.access_token.empty()) {
        return false;
    }

    time_t now;
    time(&now);

    // Check if token expires within the refresh margin
    return (current_tokens.expires_at - now) > TOKEN_REFRESH_MARGIN_SECONDS;
}

void SpotifyAuth::run_periodic_tasks() {
    if (auth_state == SpotifyAuthState::AUTHENTICATED && !is_token_valid()) {
        ESP_LOGI(TAG, "Token expired, attempting refresh");
        if (!refresh_access_token()) {
            ESP_LOGE(TAG, "Failed to refresh token");
            update_auth_state(SpotifyAuthState::TOKEN_EXPIRED);
        }
    }
}

bool SpotifyAuth::save_tokens_to_nvs() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return false;
    }

    bool success = true;

    // Save tokens
    if (nvs_set_str(nvs_handle, NVS_ACCESS_TOKEN_KEY, current_tokens.access_token.c_str()) != ESP_OK) {
        success = false;
    }
    if (nvs_set_str(nvs_handle, NVS_REFRESH_TOKEN_KEY, current_tokens.refresh_token.c_str()) != ESP_OK) {
        success = false;
    }
    if (nvs_set_str(nvs_handle, NVS_TOKEN_TYPE_KEY, current_tokens.token_type.c_str()) != ESP_OK) {
        success = false;
    }
    if (nvs_set_str(nvs_handle, NVS_SCOPE_KEY, current_tokens.scope.c_str()) != ESP_OK) {
        success = false;
    }
    if (nvs_set_u64(nvs_handle, NVS_EXPIRES_AT_KEY, current_tokens.expires_at) != ESP_OK) {
        success = false;
    }

    if (success) {
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
            success = false;
        } else {
            ESP_LOGI(TAG, "Successfully saved tokens to NVS");
        }
    } else {
        ESP_LOGE(TAG, "Failed to save tokens to NVS");
    }

    nvs_close(nvs_handle);
    return success;
}

bool SpotifyAuth::load_tokens_from_nvs() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "Failed to open NVS handle for reading: %s", esp_err_to_name(err));
        return false;
    }

    size_t required_size;
    char* buffer;
    bool success = true;

    // Load access token
    err = nvs_get_str(nvs_handle, NVS_ACCESS_TOKEN_KEY, NULL, &required_size);
    if (err == ESP_OK && required_size > 0) {
        buffer = (char*)malloc(required_size);
        if (buffer && nvs_get_str(nvs_handle, NVS_ACCESS_TOKEN_KEY, buffer, &required_size) == ESP_OK) {
            current_tokens.access_token = buffer;
        } else {
            success = false;
        }
        free(buffer);
    } else {
        success = false;
    }

    // Load refresh token
    if (success) {
        err = nvs_get_str(nvs_handle, NVS_REFRESH_TOKEN_KEY, NULL, &required_size);
        if (err == ESP_OK && required_size > 0) {
            buffer = (char*)malloc(required_size);
            if (buffer && nvs_get_str(nvs_handle, NVS_REFRESH_TOKEN_KEY, buffer, &required_size) == ESP_OK) {
                current_tokens.refresh_token = buffer;
            }
            free(buffer);
        }
    }

    // Load token type
    if (success) {
        err = nvs_get_str(nvs_handle, NVS_TOKEN_TYPE_KEY, NULL, &required_size);
        if (err == ESP_OK && required_size > 0) {
            buffer = (char*)malloc(required_size);
            if (buffer && nvs_get_str(nvs_handle, NVS_TOKEN_TYPE_KEY, buffer, &required_size) == ESP_OK) {
                current_tokens.token_type = buffer;
            }
            free(buffer);
        }
    }

    // Load scope
    if (success) {
        err = nvs_get_str(nvs_handle, NVS_SCOPE_KEY, NULL, &required_size);
        if (err == ESP_OK && required_size > 0) {
            buffer = (char*)malloc(required_size);
            if (buffer && nvs_get_str(nvs_handle, NVS_SCOPE_KEY, buffer, &required_size) == ESP_OK) {
                current_tokens.scope = buffer;
            }
            free(buffer);
        }
    }

    // Load expiration time
    if (success) {
        uint64_t expires_at;
        err = nvs_get_u64(nvs_handle, NVS_EXPIRES_AT_KEY, &expires_at);
        if (err == ESP_OK) {
            current_tokens.expires_at = expires_at;
        } else {
            success = false;
        }
    }

    nvs_close(nvs_handle);

    if (success) {
        ESP_LOGI(TAG, "Successfully loaded tokens from NVS");
    } else {
        ESP_LOGD(TAG, "Failed to load tokens from NVS");
    }

    return success;
}

void SpotifyAuth::clear_stored_tokens() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle for clearing: %s", esp_err_to_name(err));
        return;
    }

    nvs_erase_key(nvs_handle, NVS_ACCESS_TOKEN_KEY);
    nvs_erase_key(nvs_handle, NVS_REFRESH_TOKEN_KEY);
    nvs_erase_key(nvs_handle, NVS_TOKEN_TYPE_KEY);
    nvs_erase_key(nvs_handle, NVS_SCOPE_KEY);
    nvs_erase_key(nvs_handle, NVS_EXPIRES_AT_KEY);

    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Cleared stored tokens from NVS");
}
