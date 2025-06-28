#pragma once

#include <string>
#include <functional>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"

/**
 * SpotifyAuth - OAuth2 authentication with PKCE for Spotify Web API
 * 
 * Implements the Authorization Code with PKCE flow suitable for embedded devices
 * that cannot securely store client secrets.
 */

/**
 * @brief OAuth2 token information
 */
struct SpotifyTokens {
    std::string access_token;
    std::string refresh_token;
    std::string token_type;
    int expires_in;
    std::string scope;
    time_t expires_at;  // Calculated expiration timestamp
};

/**
 * @brief Authentication state
 */
enum class SpotifyAuthState {
    NOT_AUTHENTICATED,
    AUTHENTICATING,
    AUTHENTICATED,
    TOKEN_EXPIRED,
    ERROR_STATE
};

class SpotifyAuth {
public:
    // Callback function types
    using AuthStateCallback = std::function<void(SpotifyAuthState, void*)>;
    using TokenCallback = std::function<void(const SpotifyTokens&, void*)>;
    using ErrorCallback = std::function<void(const std::string&, void*)>;

private:
    // OAuth2 configuration
    std::string client_id;
    std::string redirect_uri;
    std::string scope;
    
    // PKCE parameters
    std::string code_verifier;
    std::string code_challenge;
    std::string state;
    
    // Token storage
    SpotifyTokens current_tokens;
    SpotifyAuthState auth_state;
    
    // HTTP server for callback handling
    httpd_handle_t callback_server;
    bool server_running;
    
    // Callbacks
    AuthStateCallback auth_state_callback;
    TokenCallback token_callback;
    ErrorCallback error_callback;
    void* callback_user_data;
    
    // Internal methods
    std::string generate_random_string(size_t length);
    std::string generate_code_verifier();
    std::string generate_code_challenge(const std::string& verifier);
    std::string url_encode(const std::string& value);
    bool start_callback_server();
    void stop_callback_server();
    bool exchange_code_for_tokens(const std::string& auth_code);
    bool refresh_access_token();
    void update_auth_state(SpotifyAuthState new_state);
    
    // HTTP server callback handlers
    static esp_err_t callback_handler(httpd_req_t* req);
    static esp_err_t root_handler(httpd_req_t* req);
    
public:
    SpotifyAuth();
    ~SpotifyAuth();
    
    // Initialization
    bool initialize(const std::string& client_id, 
                   const std::string& redirect_uri = "http://localhost:8888/callback",
                   const std::string& scope = "user-read-playback-state user-modify-playback-state user-read-currently-playing playlist-read-private playlist-read-collaborative");
    void deinitialize();
    
    // Authentication flow
    std::string get_authorization_url();
    bool handle_authorization_response(const std::string& auth_code, const std::string& state);
    bool refresh_token();
    void logout();
    
    // Token management
    bool is_authenticated() const;
    bool is_token_valid() const;
    const SpotifyTokens& get_tokens() const { return current_tokens; }
    std::string get_access_token() const { return current_tokens.access_token; }
    time_t get_token_expiry() const { return current_tokens.expires_at; }
    
    // State management
    SpotifyAuthState get_auth_state() const { return auth_state; }
    
    // Callback setters
    void set_auth_state_callback(AuthStateCallback callback, void* user_data = nullptr) {
        auth_state_callback = callback;
        callback_user_data = user_data;
    }
    void set_token_callback(TokenCallback callback, void* user_data = nullptr) {
        token_callback = callback;
        callback_user_data = user_data;
    }
    void set_error_callback(ErrorCallback callback, void* user_data = nullptr) {
        error_callback = callback;
        callback_user_data = user_data;
    }
    
    // Utility methods
    void run_periodic_tasks();
    bool save_tokens_to_nvs();
    bool load_tokens_from_nvs();
    void clear_stored_tokens();
    
    // Constants
    static constexpr const char* SPOTIFY_AUTH_URL = "https://accounts.spotify.com/authorize";
    static constexpr const char* SPOTIFY_TOKEN_URL = "https://accounts.spotify.com/api/token";
    static constexpr int CALLBACK_SERVER_PORT = 8888;
    static constexpr int TOKEN_REFRESH_MARGIN_SECONDS = 300; // Refresh 5 minutes before expiry
};
