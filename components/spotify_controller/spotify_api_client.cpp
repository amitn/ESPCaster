#include "spotify_api_client.h"
#include "esp_log.h"
#include <cstring>
#include <sstream>

static const char *TAG = "spotify_api_client";

SpotifyApiClient::SpotifyApiClient() 
    : http_client(nullptr)
    , base_url(SPOTIFY_API_BASE_URL)
    , response_callback(nullptr)
    , playback_callback(nullptr)
    , playlists_callback(nullptr)
    , tracks_callback(nullptr)
    , devices_callback(nullptr)
    , error_callback(nullptr)
    , callback_user_data(nullptr)
    , last_request_time(0)
    , requests_per_second_limit(DEFAULT_RATE_LIMIT) {
}

SpotifyApiClient::~SpotifyApiClient() {
    deinitialize();
}

bool SpotifyApiClient::initialize(const std::string& access_token) {
    ESP_LOGI(TAG, "Initializing Spotify API client");
    
    this->access_token = access_token;
    
    if (!setup_http_client()) {
        ESP_LOGE(TAG, "Failed to setup HTTP client");
        return false;
    }
    
    ESP_LOGI(TAG, "Spotify API client initialized successfully");
    return true;
}

void SpotifyApiClient::deinitialize() {
    ESP_LOGI(TAG, "Deinitializing Spotify API client");
    cleanup_http_client();
    access_token.clear();
}

void SpotifyApiClient::set_access_token(const std::string& token) {
    access_token = token;
    ESP_LOGI(TAG, "Access token updated");
}

bool SpotifyApiClient::setup_http_client() {
    esp_http_client_config_t config = {};
    config.url = base_url.c_str();
    config.event_handler = http_event_handler;
    config.user_data = this;
    config.timeout_ms = HTTP_TIMEOUT_MS;
    config.buffer_size = 4096;
    config.buffer_size_tx = 2048;
    
    http_client = esp_http_client_init(&config);
    if (!http_client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return false;
    }
    
    return true;
}

void SpotifyApiClient::cleanup_http_client() {
    if (http_client) {
        esp_http_client_cleanup(http_client);
        http_client = nullptr;
    }
}

std::string SpotifyApiClient::build_url(const std::string& endpoint) {
    if (endpoint.empty()) {
        return base_url;
    }
    
    std::string url = base_url;
    if (endpoint[0] != '/') {
        url += "/";
    }
    url += endpoint;
    
    return url;
}

bool SpotifyApiClient::add_auth_header() {
    if (access_token.empty()) {
        ESP_LOGE(TAG, "No access token available");
        return false;
    }
    
    std::string auth_header = "Bearer " + access_token;
    esp_err_t err = esp_http_client_set_header(http_client, "Authorization", auth_header.c_str());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set authorization header: %s", esp_err_to_name(err));
        return false;
    }
    
    return true;
}

bool SpotifyApiClient::check_rate_limit() {
    time_t now;
    time(&now);
    
    // Simple rate limiting - ensure minimum time between requests
    if (now == last_request_time) {
        // Wait a bit if making requests too quickly
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    last_request_time = now;
    return true;
}

SpotifyApiResponse SpotifyApiClient::make_request(const SpotifyApiRequest& request) {
    SpotifyApiResponse response = {};
    response.success = false;
    
    if (!http_client) {
        response.error_message = "HTTP client not initialized";
        ESP_LOGE(TAG, "%s", response.error_message.c_str());
        return response;
    }
    
    // Check rate limit
    if (!check_rate_limit()) {
        response.error_message = "Rate limit exceeded";
        ESP_LOGE(TAG, "%s", response.error_message.c_str());
        return response;
    }
    
    // Build full URL
    std::string url = build_url(request.endpoint);
    esp_http_client_set_url(http_client, url.c_str());
    
    // Set HTTP method
    if (request.method == "GET") {
        esp_http_client_set_method(http_client, HTTP_METHOD_GET);
    } else if (request.method == "POST") {
        esp_http_client_set_method(http_client, HTTP_METHOD_POST);
    } else if (request.method == "PUT") {
        esp_http_client_set_method(http_client, HTTP_METHOD_PUT);
    } else if (request.method == "DELETE") {
        esp_http_client_set_method(http_client, HTTP_METHOD_DELETE);
    } else {
        response.error_message = "Unsupported HTTP method: " + request.method;
        ESP_LOGE(TAG, "%s", response.error_message.c_str());
        return response;
    }
    
    // Set headers
    esp_http_client_set_header(http_client, "Content-Type", "application/json");
    
    if (request.requires_auth && !add_auth_header()) {
        response.error_message = "Failed to add authorization header";
        return response;
    }
    
    // Set request body for POST/PUT
    if (!request.body.empty() && (request.method == "POST" || request.method == "PUT")) {
        esp_http_client_set_post_field(http_client, request.body.c_str(), request.body.length());
    }
    
    // Perform request
    esp_err_t err = esp_http_client_perform(http_client);
    if (err != ESP_OK) {
        response.error_message = "HTTP request failed: " + std::string(esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", response.error_message.c_str());
        return response;
    }
    
    // Get response status
    response.status_code = esp_http_client_get_status_code(http_client);
    int content_length = esp_http_client_get_content_length(http_client);
    
    ESP_LOGD(TAG, "API request: %s %s -> %d (%d bytes)", 
             request.method.c_str(), request.endpoint.c_str(), 
             response.status_code, content_length);
    
    // Read response body
    if (content_length > 0) {
        char* buffer = (char*)malloc(content_length + 1);
        if (buffer) {
            int data_read = esp_http_client_read_response(http_client, buffer, content_length);
            buffer[data_read] = '\0';
            response.body = buffer;
            free(buffer);
        } else {
            response.error_message = "Failed to allocate response buffer";
            ESP_LOGE(TAG, "%s", response.error_message.c_str());
            return response;
        }
    }
    
    // Check if request was successful
    if (response.status_code >= 200 && response.status_code < 300) {
        response.success = true;
    } else {
        response.success = false;
        handle_api_error(response.status_code, response.body);
    }
    
    return response;
}

void SpotifyApiClient::handle_api_error(int status_code, const std::string& response_body) {
    std::string error_message = "API error " + std::to_string(status_code);
    
    // Try to parse error details from response
    if (!response_body.empty()) {
        cJSON* json = cJSON_Parse(response_body.c_str());
        if (json) {
            cJSON* error = cJSON_GetObjectItem(json, "error");
            if (error) {
                cJSON* message = cJSON_GetObjectItem(error, "message");
                if (message && cJSON_IsString(message)) {
                    error_message += ": " + std::string(cJSON_GetStringValue(message));
                }
            }
            cJSON_Delete(json);
        }
    }
    
    ESP_LOGE(TAG, "%s", error_message.c_str());
    
    if (error_callback) {
        error_callback(error_message, callback_user_data);
    }
}

esp_err_t SpotifyApiClient::http_event_handler(esp_http_client_event_t* evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Player API methods
bool SpotifyApiClient::get_playback_state() {
    SpotifyApiRequest request = {
        .method = "GET",
        .endpoint = "/me/player",
        .body = "",
        .requires_auth = true
    };

    SpotifyApiResponse response = make_request(request);
    if (!response.success) {
        return false;
    }

    // Handle empty response (no active device)
    if (response.body.empty() || response.status_code == 204) {
        ESP_LOGI(TAG, "No active playback device");
        return true;
    }

    // Parse playback state
    cJSON* json = cJSON_Parse(response.body.c_str());
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse playback state JSON");
        return false;
    }

    SpotifyPlaybackState state = parse_playback_state(json);
    cJSON_Delete(json);

    if (playback_callback) {
        playback_callback(state, callback_user_data);
    }

    return true;
}

bool SpotifyApiClient::start_resume_playback(const std::string& device_id, const std::string& context_uri) {
    std::string endpoint = "/me/player/play";
    if (!device_id.empty()) {
        endpoint += "?device_id=" + device_id;
    }

    std::string body = "{}";
    if (!context_uri.empty()) {
        cJSON* json = cJSON_CreateObject();
        if (context_uri.find("spotify:track:") == 0) {
            // Single track
            cJSON* uris = cJSON_CreateArray();
            cJSON_AddItemToArray(uris, cJSON_CreateString(context_uri.c_str()));
            cJSON_AddItemToObject(json, "uris", uris);
        } else {
            // Context (playlist, album, etc.)
            cJSON_AddStringToObject(json, "context_uri", context_uri.c_str());
        }

        char* json_string = cJSON_Print(json);
        body = json_string;
        free(json_string);
        cJSON_Delete(json);
    }

    SpotifyApiRequest request = {
        .method = "PUT",
        .endpoint = endpoint,
        .body = body,
        .requires_auth = true
    };

    SpotifyApiResponse response = make_request(request);
    return response.success;
}

bool SpotifyApiClient::pause_playback(const std::string& device_id) {
    std::string endpoint = "/me/player/pause";
    if (!device_id.empty()) {
        endpoint += "?device_id=" + device_id;
    }

    SpotifyApiRequest request = {
        .method = "PUT",
        .endpoint = endpoint,
        .body = "",
        .requires_auth = true
    };

    SpotifyApiResponse response = make_request(request);
    return response.success;
}

bool SpotifyApiClient::skip_to_next(const std::string& device_id) {
    std::string endpoint = "/me/player/next";
    if (!device_id.empty()) {
        endpoint += "?device_id=" + device_id;
    }

    SpotifyApiRequest request = {
        .method = "POST",
        .endpoint = endpoint,
        .body = "",
        .requires_auth = true
    };

    SpotifyApiResponse response = make_request(request);
    return response.success;
}

bool SpotifyApiClient::skip_to_previous(const std::string& device_id) {
    std::string endpoint = "/me/player/previous";
    if (!device_id.empty()) {
        endpoint += "?device_id=" + device_id;
    }

    SpotifyApiRequest request = {
        .method = "POST",
        .endpoint = endpoint,
        .body = "",
        .requires_auth = true
    };

    SpotifyApiResponse response = make_request(request);
    return response.success;
}

bool SpotifyApiClient::seek_to_position(int position_ms, const std::string& device_id) {
    std::string endpoint = "/me/player/seek?position_ms=" + std::to_string(position_ms);
    if (!device_id.empty()) {
        endpoint += "&device_id=" + device_id;
    }

    SpotifyApiRequest request = {
        .method = "PUT",
        .endpoint = endpoint,
        .body = "",
        .requires_auth = true
    };

    SpotifyApiResponse response = make_request(request);
    return response.success;
}

bool SpotifyApiClient::set_repeat_mode(const std::string& state, const std::string& device_id) {
    std::string endpoint = "/me/player/repeat?state=" + state;
    if (!device_id.empty()) {
        endpoint += "&device_id=" + device_id;
    }

    SpotifyApiRequest request = {
        .method = "PUT",
        .endpoint = endpoint,
        .body = "",
        .requires_auth = true
    };

    SpotifyApiResponse response = make_request(request);
    return response.success;
}

bool SpotifyApiClient::set_playback_volume(int volume_percent, const std::string& device_id) {
    std::string endpoint = "/me/player/volume?volume_percent=" + std::to_string(volume_percent);
    if (!device_id.empty()) {
        endpoint += "&device_id=" + device_id;
    }

    SpotifyApiRequest request = {
        .method = "PUT",
        .endpoint = endpoint,
        .body = "",
        .requires_auth = true
    };

    SpotifyApiResponse response = make_request(request);
    return response.success;
}

bool SpotifyApiClient::toggle_playback_shuffle(bool state, const std::string& device_id) {
    std::string endpoint = "/me/player/shuffle?state=" + std::string(state ? "true" : "false");
    if (!device_id.empty()) {
        endpoint += "&device_id=" + device_id;
    }

    SpotifyApiRequest request = {
        .method = "PUT",
        .endpoint = endpoint,
        .body = "",
        .requires_auth = true
    };

    SpotifyApiResponse response = make_request(request);
    return response.success;
}

bool SpotifyApiClient::transfer_playback(const std::string& device_id, bool play) {
    cJSON* json = cJSON_CreateObject();
    cJSON* device_ids = cJSON_CreateArray();
    cJSON_AddItemToArray(device_ids, cJSON_CreateString(device_id.c_str()));
    cJSON_AddItemToObject(json, "device_ids", device_ids);
    cJSON_AddBoolToObject(json, "play", play);

    char* json_string = cJSON_Print(json);
    std::string body = json_string;
    free(json_string);
    cJSON_Delete(json);

    SpotifyApiRequest request = {
        .method = "PUT",
        .endpoint = "/me/player",
        .body = body,
        .requires_auth = true
    };

    SpotifyApiResponse response = make_request(request);
    return response.success;
}

bool SpotifyApiClient::add_to_queue(const std::string& uri, const std::string& device_id) {
    std::string endpoint = "/me/player/queue?uri=" + uri;
    if (!device_id.empty()) {
        endpoint += "&device_id=" + device_id;
    }

    SpotifyApiRequest request = {
        .method = "POST",
        .endpoint = endpoint,
        .body = "",
        .requires_auth = true
    };

    SpotifyApiResponse response = make_request(request);
    return response.success;
}

// Device API methods
bool SpotifyApiClient::get_available_devices() {
    SpotifyApiRequest request = {
        .method = "GET",
        .endpoint = "/me/player/devices",
        .body = "",
        .requires_auth = true
    };

    SpotifyApiResponse response = make_request(request);
    if (!response.success) {
        return false;
    }

    cJSON* json = cJSON_Parse(response.body.c_str());
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse devices JSON");
        return false;
    }

    std::vector<SpotifyDevice> devices = parse_devices(json);
    cJSON_Delete(json);

    if (devices_callback) {
        devices_callback(devices, callback_user_data);
    }

    return true;
}

// Playlist API methods
bool SpotifyApiClient::get_user_playlists(const std::string& user_id, int limit, int offset) {
    std::string endpoint = "/" + user_id + "/playlists?limit=" + std::to_string(limit) + "&offset=" + std::to_string(offset);

    SpotifyApiRequest request = {
        .method = "GET",
        .endpoint = endpoint,
        .body = "",
        .requires_auth = true
    };

    SpotifyApiResponse response = make_request(request);
    if (!response.success) {
        return false;
    }

    cJSON* json = cJSON_Parse(response.body.c_str());
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse playlists JSON");
        return false;
    }

    std::vector<SpotifyPlaylist> playlists = parse_playlists(json);
    cJSON_Delete(json);

    if (playlists_callback) {
        playlists_callback(playlists, callback_user_data);
    }

    return true;
}

bool SpotifyApiClient::get_playlist_tracks(const std::string& playlist_id, int limit, int offset) {
    std::string endpoint = "/playlists/" + playlist_id + "/tracks?limit=" + std::to_string(limit) + "&offset=" + std::to_string(offset);

    SpotifyApiRequest request = {
        .method = "GET",
        .endpoint = endpoint,
        .body = "",
        .requires_auth = true
    };

    SpotifyApiResponse response = make_request(request);
    if (!response.success) {
        return false;
    }

    cJSON* json = cJSON_Parse(response.body.c_str());
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse playlist tracks JSON");
        return false;
    }

    std::vector<SpotifyTrack> tracks = parse_tracks(json);
    cJSON_Delete(json);

    if (tracks_callback) {
        tracks_callback(tracks, callback_user_data);
    }

    return true;
}

bool SpotifyApiClient::search(const std::string& query, const std::string& type, int limit, int offset) {
    std::string endpoint = "/search?q=" + query + "&type=" + type + "&limit=" + std::to_string(limit) + "&offset=" + std::to_string(offset);

    SpotifyApiRequest request = {
        .method = "GET",
        .endpoint = endpoint,
        .body = "",
        .requires_auth = true
    };

    SpotifyApiResponse response = make_request(request);
    if (!response.success) {
        return false;
    }

    cJSON* json = cJSON_Parse(response.body.c_str());
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse search results JSON");
        return false;
    }

    // Parse tracks from search results
    cJSON* tracks_obj = cJSON_GetObjectItem(json, "tracks");
    if (tracks_obj) {
        std::vector<SpotifyTrack> tracks = parse_tracks(tracks_obj);
        if (tracks_callback) {
            tracks_callback(tracks, callback_user_data);
        }
    }

    cJSON_Delete(json);
    return true;
}

// JSON parsing helper functions
SpotifyPlaybackState SpotifyApiClient::parse_playback_state(cJSON* json) {
    SpotifyPlaybackState state = {};

    if (!json) return state;

    cJSON* is_playing = cJSON_GetObjectItem(json, "is_playing");
    if (is_playing && cJSON_IsBool(is_playing)) {
        state.is_playing = cJSON_IsTrue(is_playing);
    }

    cJSON* progress_ms = cJSON_GetObjectItem(json, "progress_ms");
    if (progress_ms && cJSON_IsNumber(progress_ms)) {
        state.progress_ms = cJSON_GetNumberValue(progress_ms);
    }

    cJSON* shuffle_state = cJSON_GetObjectItem(json, "shuffle_state");
    if (shuffle_state && cJSON_IsBool(shuffle_state)) {
        state.shuffle_state = cJSON_IsTrue(shuffle_state);
    }

    cJSON* repeat_state = cJSON_GetObjectItem(json, "repeat_state");
    if (repeat_state && cJSON_IsString(repeat_state)) {
        state.repeat_state = cJSON_GetStringValue(repeat_state);
    }

    cJSON* device = cJSON_GetObjectItem(json, "device");
    if (device) {
        cJSON* device_id = cJSON_GetObjectItem(device, "id");
        if (device_id && cJSON_IsString(device_id)) {
            state.device_id = cJSON_GetStringValue(device_id);
        }

        cJSON* device_name = cJSON_GetObjectItem(device, "name");
        if (device_name && cJSON_IsString(device_name)) {
            state.device_name = cJSON_GetStringValue(device_name);
        }

        cJSON* volume_percent = cJSON_GetObjectItem(device, "volume_percent");
        if (volume_percent && cJSON_IsNumber(volume_percent)) {
            state.volume_percent = cJSON_GetNumberValue(volume_percent);
        }
    }

    cJSON* item = cJSON_GetObjectItem(json, "item");
    if (item) {
        state.current_track = parse_track(item);
    }

    return state;
}

std::vector<SpotifyPlaylist> SpotifyApiClient::parse_playlists(cJSON* json) {
    std::vector<SpotifyPlaylist> playlists;

    if (!json) return playlists;

    cJSON* items = cJSON_GetObjectItem(json, "items");
    if (!items || !cJSON_IsArray(items)) return playlists;

    cJSON* item;
    cJSON_ArrayForEach(item, items) {
        SpotifyPlaylist playlist = {};

        cJSON* id = cJSON_GetObjectItem(item, "id");
        if (id && cJSON_IsString(id)) {
            playlist.id = cJSON_GetStringValue(id);
        }

        cJSON* name = cJSON_GetObjectItem(item, "name");
        if (name && cJSON_IsString(name)) {
            playlist.name = cJSON_GetStringValue(name);
        }

        cJSON* description = cJSON_GetObjectItem(item, "description");
        if (description && cJSON_IsString(description)) {
            playlist.description = cJSON_GetStringValue(description);
        }

        cJSON* uri = cJSON_GetObjectItem(item, "uri");
        if (uri && cJSON_IsString(uri)) {
            playlist.uri = cJSON_GetStringValue(uri);
        }

        cJSON* tracks = cJSON_GetObjectItem(item, "tracks");
        if (tracks) {
            cJSON* total = cJSON_GetObjectItem(tracks, "total");
            if (total && cJSON_IsNumber(total)) {
                playlist.track_count = cJSON_GetNumberValue(total);
            }
        }

        cJSON* images = cJSON_GetObjectItem(item, "images");
        if (images && cJSON_IsArray(images)) {
            cJSON* first_image = cJSON_GetArrayItem(images, 0);
            if (first_image) {
                cJSON* url = cJSON_GetObjectItem(first_image, "url");
                if (url && cJSON_IsString(url)) {
                    playlist.image_url = cJSON_GetStringValue(url);
                }
            }
        }

        cJSON* owner = cJSON_GetObjectItem(item, "owner");
        if (owner) {
            cJSON* display_name = cJSON_GetObjectItem(owner, "display_name");
            if (display_name && cJSON_IsString(display_name)) {
                playlist.owner = cJSON_GetStringValue(display_name);
            }
        }

        playlists.push_back(playlist);
    }

    return playlists;
}

std::vector<SpotifyTrack> SpotifyApiClient::parse_tracks(cJSON* json) {
    std::vector<SpotifyTrack> tracks;

    if (!json) return tracks;

    cJSON* items = cJSON_GetObjectItem(json, "items");
    if (!items || !cJSON_IsArray(items)) return tracks;

    cJSON* item;
    cJSON_ArrayForEach(item, items) {
        // Handle both direct track objects and playlist track objects
        cJSON* track_obj = cJSON_GetObjectItem(item, "track");
        if (!track_obj) {
            track_obj = item; // Direct track object
        }

        if (track_obj) {
            SpotifyTrack track = parse_track(track_obj);
            tracks.push_back(track);
        }
    }

    return tracks;
}

std::vector<SpotifyDevice> SpotifyApiClient::parse_devices(cJSON* json) {
    std::vector<SpotifyDevice> devices;

    if (!json) return devices;

    cJSON* devices_array = cJSON_GetObjectItem(json, "devices");
    if (!devices_array || !cJSON_IsArray(devices_array)) return devices;

    cJSON* device;
    cJSON_ArrayForEach(device, devices_array) {
        SpotifyDevice dev = {};

        cJSON* id = cJSON_GetObjectItem(device, "id");
        if (id && cJSON_IsString(id)) {
            dev.id = cJSON_GetStringValue(id);
        }

        cJSON* name = cJSON_GetObjectItem(device, "name");
        if (name && cJSON_IsString(name)) {
            dev.name = cJSON_GetStringValue(name);
        }

        cJSON* type = cJSON_GetObjectItem(device, "type");
        if (type && cJSON_IsString(type)) {
            dev.type = cJSON_GetStringValue(type);
        }

        cJSON* is_active = cJSON_GetObjectItem(device, "is_active");
        if (is_active && cJSON_IsBool(is_active)) {
            dev.is_active = cJSON_IsTrue(is_active);
        }

        cJSON* is_private_session = cJSON_GetObjectItem(device, "is_private_session");
        if (is_private_session && cJSON_IsBool(is_private_session)) {
            dev.is_private_session = cJSON_IsTrue(is_private_session);
        }

        cJSON* is_restricted = cJSON_GetObjectItem(device, "is_restricted");
        if (is_restricted && cJSON_IsBool(is_restricted)) {
            dev.is_restricted = cJSON_IsTrue(is_restricted);
        }

        cJSON* volume_percent = cJSON_GetObjectItem(device, "volume_percent");
        if (volume_percent && cJSON_IsNumber(volume_percent)) {
            dev.volume_percent = cJSON_GetNumberValue(volume_percent);
        }

        devices.push_back(dev);
    }

    return devices;
}

SpotifyTrack SpotifyApiClient::parse_track(cJSON* track_json) {
    SpotifyTrack track = {};

    if (!track_json) return track;

    cJSON* id = cJSON_GetObjectItem(track_json, "id");
    if (id && cJSON_IsString(id)) {
        track.id = cJSON_GetStringValue(id);
    }

    cJSON* name = cJSON_GetObjectItem(track_json, "name");
    if (name && cJSON_IsString(name)) {
        track.name = cJSON_GetStringValue(name);
    }

    cJSON* uri = cJSON_GetObjectItem(track_json, "uri");
    if (uri && cJSON_IsString(uri)) {
        track.uri = cJSON_GetStringValue(uri);
    }

    cJSON* duration_ms = cJSON_GetObjectItem(track_json, "duration_ms");
    if (duration_ms && cJSON_IsNumber(duration_ms)) {
        track.duration_ms = cJSON_GetNumberValue(duration_ms);
    }

    cJSON* preview_url = cJSON_GetObjectItem(track_json, "preview_url");
    if (preview_url && cJSON_IsString(preview_url)) {
        track.preview_url = cJSON_GetStringValue(preview_url);
    }

    // Parse artists
    cJSON* artists = cJSON_GetObjectItem(track_json, "artists");
    if (artists && cJSON_IsArray(artists)) {
        cJSON* first_artist = cJSON_GetArrayItem(artists, 0);
        if (first_artist) {
            cJSON* artist_name = cJSON_GetObjectItem(first_artist, "name");
            if (artist_name && cJSON_IsString(artist_name)) {
                track.artist = cJSON_GetStringValue(artist_name);
            }
        }
    }

    // Parse album
    cJSON* album = cJSON_GetObjectItem(track_json, "album");
    if (album) {
        cJSON* album_name = cJSON_GetObjectItem(album, "name");
        if (album_name && cJSON_IsString(album_name)) {
            track.album = cJSON_GetStringValue(album_name);
        }

        cJSON* images = cJSON_GetObjectItem(album, "images");
        if (images && cJSON_IsArray(images)) {
            cJSON* first_image = cJSON_GetArrayItem(images, 0);
            if (first_image) {
                cJSON* url = cJSON_GetObjectItem(first_image, "url");
                if (url && cJSON_IsString(url)) {
                    track.image_url = cJSON_GetStringValue(url);
                }
            }
        }
    }

    return track;
}

std::string SpotifyApiClient::get_last_error() const {
    // This could be enhanced to store the last error message
    return "Check logs for error details";
}
