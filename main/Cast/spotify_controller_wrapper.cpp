#include "spotify_controller_wrapper.h"
#include "spotify_controller.h"
#include "spotify_auth.h"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "spotify_wrapper";

// Internal structure to hold C++ instance and callbacks
struct spotify_controller_wrapper {
    SpotifyController* controller;
    spotify_auth_state_callback_t auth_state_callback;
    spotify_connection_state_callback_t connection_state_callback;
    spotify_playback_state_callback_t playback_state_callback;
    spotify_playlists_callback_t playlists_callback;
    spotify_tracks_callback_t tracks_callback;
    spotify_devices_callback_t devices_callback;
    spotify_error_callback_t error_callback;
};

// Helper functions to convert between C++ and C structures
static void convert_playback_state(const SpotifyPlaybackState& cpp_state, spotify_playback_state_t* c_state) {
    if (!c_state) return;
    
    memset(c_state, 0, sizeof(spotify_playback_state_t));
    
    c_state->is_playing = cpp_state.is_playing;
    c_state->progress_ms = cpp_state.progress_ms;
    c_state->volume_percent = cpp_state.volume_percent;
    c_state->shuffle_state = cpp_state.shuffle_state;
    
    strncpy(c_state->repeat_state, cpp_state.repeat_state.c_str(), sizeof(c_state->repeat_state) - 1);
    strncpy(c_state->device_id, cpp_state.device_id.c_str(), sizeof(c_state->device_id) - 1);
    strncpy(c_state->device_name, cpp_state.device_name.c_str(), sizeof(c_state->device_name) - 1);
    
    // Convert track info
    strncpy(c_state->current_track.id, cpp_state.current_track.id.c_str(), sizeof(c_state->current_track.id) - 1);
    strncpy(c_state->current_track.name, cpp_state.current_track.name.c_str(), sizeof(c_state->current_track.name) - 1);
    strncpy(c_state->current_track.artist, cpp_state.current_track.artist.c_str(), sizeof(c_state->current_track.artist) - 1);
    strncpy(c_state->current_track.album, cpp_state.current_track.album.c_str(), sizeof(c_state->current_track.album) - 1);
    strncpy(c_state->current_track.uri, cpp_state.current_track.uri.c_str(), sizeof(c_state->current_track.uri) - 1);
    strncpy(c_state->current_track.preview_url, cpp_state.current_track.preview_url.c_str(), sizeof(c_state->current_track.preview_url) - 1);
    strncpy(c_state->current_track.image_url, cpp_state.current_track.image_url.c_str(), sizeof(c_state->current_track.image_url) - 1);
    c_state->current_track.duration_ms = cpp_state.current_track.duration_ms;
}

static spotify_auth_state_t convert_auth_state(SpotifyAuthState cpp_state) {
    switch (cpp_state) {
        case SpotifyAuthState::NOT_AUTHENTICATED:
            return SPOTIFY_AUTH_NOT_AUTHENTICATED;
        case SpotifyAuthState::AUTHENTICATING:
            return SPOTIFY_AUTH_AUTHENTICATING;
        case SpotifyAuthState::AUTHENTICATED:
            return SPOTIFY_AUTH_AUTHENTICATED;
        case SpotifyAuthState::TOKEN_EXPIRED:
            return SPOTIFY_AUTH_TOKEN_EXPIRED;
        case SpotifyAuthState::ERROR_STATE:
            return SPOTIFY_AUTH_ERROR_STATE;
        default:
            return SPOTIFY_AUTH_ERROR_STATE;
    }
}

static spotify_connection_state_t convert_connection_state(SpotifyConnectionState cpp_state) {
    switch (cpp_state) {
        case SpotifyConnectionState::DISCONNECTED:
            return SPOTIFY_CONNECTION_DISCONNECTED;
        case SpotifyConnectionState::CONNECTING:
            return SPOTIFY_CONNECTION_CONNECTING;
        case SpotifyConnectionState::CONNECTED:
            return SPOTIFY_CONNECTION_CONNECTED;
        case SpotifyConnectionState::ERROR_STATE:
            return SPOTIFY_CONNECTION_ERROR_STATE;
        default:
            return SPOTIFY_CONNECTION_ERROR_STATE;
    }
}

// C API implementation
spotify_controller_handle_t spotify_controller_create(void) {
    ESP_LOGI(TAG, "Creating Spotify controller");
    
    spotify_controller_wrapper* wrapper = new(std::nothrow) spotify_controller_wrapper;
    if (!wrapper) {
        ESP_LOGE(TAG, "Failed to allocate wrapper");
        return nullptr;
    }
    
    wrapper->controller = new(std::nothrow) SpotifyController();
    if (!wrapper->controller) {
        ESP_LOGE(TAG, "Failed to create SpotifyController");
        delete wrapper;
        return nullptr;
    }
    
    // Initialize callbacks to nullptr
    wrapper->auth_state_callback = nullptr;
    wrapper->connection_state_callback = nullptr;
    wrapper->playback_state_callback = nullptr;
    wrapper->playlists_callback = nullptr;
    wrapper->tracks_callback = nullptr;
    wrapper->devices_callback = nullptr;
    wrapper->error_callback = nullptr;
    
    return wrapper;
}

void spotify_controller_destroy(spotify_controller_handle_t handle) {
    if (!handle) return;
    
    ESP_LOGI(TAG, "Destroying Spotify controller");
    
    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    
    if (wrapper->controller) {
        delete wrapper->controller;
    }
    
    delete wrapper;
}

bool spotify_controller_initialize(spotify_controller_handle_t handle, 
                                 const char* client_id, 
                                 const char* client_secret,
                                 const char* redirect_uri) {
    if (!handle || !client_id) {
        ESP_LOGE(TAG, "Invalid parameters for initialization");
        return false;
    }
    
    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    
    std::string client_secret_str = client_secret ? client_secret : "";
    std::string redirect_uri_str = redirect_uri ? redirect_uri : "http://localhost:8888/callback";
    
    bool result = wrapper->controller->initialize(client_id, client_secret_str, redirect_uri_str);
    
    if (result) {
        // Set up C++ callbacks that will call C callbacks
        wrapper->controller->set_auth_state_callback([wrapper](SpotifyAuthState state) {
            if (wrapper->auth_state_callback) {
                wrapper->auth_state_callback(convert_auth_state(state));
            }
        });
        
        wrapper->controller->set_connection_state_callback([wrapper](SpotifyConnectionState state) {
            if (wrapper->connection_state_callback) {
                wrapper->connection_state_callback(convert_connection_state(state));
            }
        });
        
        wrapper->controller->set_playback_state_callback([wrapper](const SpotifyPlaybackState& state) {
            if (wrapper->playback_state_callback) {
                spotify_playback_state_t c_state;
                convert_playback_state(state, &c_state);
                wrapper->playback_state_callback(&c_state);
            }
        });
        
        wrapper->controller->set_playlists_callback([wrapper](const std::vector<SpotifyPlaylist>& playlists) {
            if (wrapper->playlists_callback && !playlists.empty()) {
                // Convert to C array
                spotify_playlist_info_t* c_playlists = new spotify_playlist_info_t[playlists.size()];
                for (size_t i = 0; i < playlists.size(); ++i) {
                    memset(&c_playlists[i], 0, sizeof(spotify_playlist_info_t));
                    strncpy(c_playlists[i].id, playlists[i].id.c_str(), sizeof(c_playlists[i].id) - 1);
                    strncpy(c_playlists[i].name, playlists[i].name.c_str(), sizeof(c_playlists[i].name) - 1);
                    strncpy(c_playlists[i].description, playlists[i].description.c_str(), sizeof(c_playlists[i].description) - 1);
                    strncpy(c_playlists[i].uri, playlists[i].uri.c_str(), sizeof(c_playlists[i].uri) - 1);
                    strncpy(c_playlists[i].image_url, playlists[i].image_url.c_str(), sizeof(c_playlists[i].image_url) - 1);
                    strncpy(c_playlists[i].owner, playlists[i].owner.c_str(), sizeof(c_playlists[i].owner) - 1);
                    c_playlists[i].track_count = playlists[i].track_count;
                }
                wrapper->playlists_callback(c_playlists, playlists.size());
                delete[] c_playlists;
            }
        });
        
        wrapper->controller->set_tracks_callback([wrapper](const std::vector<SpotifyTrack>& tracks) {
            if (wrapper->tracks_callback && !tracks.empty()) {
                // Convert to C array
                spotify_track_info_t* c_tracks = new spotify_track_info_t[tracks.size()];
                for (size_t i = 0; i < tracks.size(); ++i) {
                    memset(&c_tracks[i], 0, sizeof(spotify_track_info_t));
                    strncpy(c_tracks[i].id, tracks[i].id.c_str(), sizeof(c_tracks[i].id) - 1);
                    strncpy(c_tracks[i].name, tracks[i].name.c_str(), sizeof(c_tracks[i].name) - 1);
                    strncpy(c_tracks[i].artist, tracks[i].artist.c_str(), sizeof(c_tracks[i].artist) - 1);
                    strncpy(c_tracks[i].album, tracks[i].album.c_str(), sizeof(c_tracks[i].album) - 1);
                    strncpy(c_tracks[i].uri, tracks[i].uri.c_str(), sizeof(c_tracks[i].uri) - 1);
                    strncpy(c_tracks[i].preview_url, tracks[i].preview_url.c_str(), sizeof(c_tracks[i].preview_url) - 1);
                    strncpy(c_tracks[i].image_url, tracks[i].image_url.c_str(), sizeof(c_tracks[i].image_url) - 1);
                    c_tracks[i].duration_ms = tracks[i].duration_ms;
                }
                wrapper->tracks_callback(c_tracks, tracks.size());
                delete[] c_tracks;
            }
        });
        
        wrapper->controller->set_devices_callback([wrapper](const std::vector<SpotifyDevice>& devices) {
            if (wrapper->devices_callback && !devices.empty()) {
                // Convert to C array
                spotify_device_info_t* c_devices = new spotify_device_info_t[devices.size()];
                for (size_t i = 0; i < devices.size(); ++i) {
                    memset(&c_devices[i], 0, sizeof(spotify_device_info_t));
                    strncpy(c_devices[i].id, devices[i].id.c_str(), sizeof(c_devices[i].id) - 1);
                    strncpy(c_devices[i].name, devices[i].name.c_str(), sizeof(c_devices[i].name) - 1);
                    strncpy(c_devices[i].type, devices[i].type.c_str(), sizeof(c_devices[i].type) - 1);
                    c_devices[i].is_active = devices[i].is_active;
                    c_devices[i].is_private_session = devices[i].is_private_session;
                    c_devices[i].is_restricted = devices[i].is_restricted;
                    c_devices[i].volume_percent = devices[i].volume_percent;
                }
                wrapper->devices_callback(c_devices, devices.size());
                delete[] c_devices;
            }
        });
        
        wrapper->controller->set_error_callback([wrapper](const std::string& error) {
            if (wrapper->error_callback) {
                wrapper->error_callback(error.c_str());
            }
        });
    }
    
    return result;
}

void spotify_controller_deinitialize(spotify_controller_handle_t handle) {
    if (!handle) return;
    
    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    wrapper->controller->deinitialize();
}

bool spotify_controller_start_authentication(spotify_controller_handle_t handle) {
    if (!handle) return false;
    
    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return wrapper->controller->start_authentication();
}

bool spotify_controller_get_auth_url(spotify_controller_handle_t handle, char* url_buffer, size_t buffer_size) {
    if (!handle || !url_buffer || buffer_size == 0) return false;
    
    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    std::string url = wrapper->controller->get_auth_url();
    
    if (url.empty()) return false;
    
    strncpy(url_buffer, url.c_str(), buffer_size - 1);
    url_buffer[buffer_size - 1] = '\0';
    
    return true;
}

bool spotify_controller_complete_authentication(spotify_controller_handle_t handle, const char* auth_code) {
    if (!handle || !auth_code) return false;
    
    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return wrapper->controller->complete_authentication(auth_code);
}

bool spotify_controller_is_authenticated(spotify_controller_handle_t handle) {
    if (!handle) return false;
    
    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return wrapper->controller->is_authenticated();
}

bool spotify_controller_connect(spotify_controller_handle_t handle) {
    if (!handle) return false;
    
    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return wrapper->controller->connect();
}

void spotify_controller_disconnect(spotify_controller_handle_t handle) {
    if (!handle) return;
    
    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    wrapper->controller->disconnect();
}

bool spotify_controller_is_connected(spotify_controller_handle_t handle) {
    if (!handle) return false;
    
    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return wrapper->controller->is_connected();
}

// Playback control functions
bool spotify_controller_play(spotify_controller_handle_t handle, const char* uri) {
    if (!handle) return false;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    std::string uri_str = uri ? uri : "";
    return wrapper->controller->play(uri_str);
}

bool spotify_controller_pause(spotify_controller_handle_t handle) {
    if (!handle) return false;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return wrapper->controller->pause();
}

bool spotify_controller_next_track(spotify_controller_handle_t handle) {
    if (!handle) return false;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return wrapper->controller->next_track();
}

bool spotify_controller_previous_track(spotify_controller_handle_t handle) {
    if (!handle) return false;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return wrapper->controller->previous_track();
}

bool spotify_controller_set_volume(spotify_controller_handle_t handle, int volume_percent) {
    if (!handle) return false;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return wrapper->controller->set_volume(volume_percent);
}

// Content functions
bool spotify_controller_get_playlists(spotify_controller_handle_t handle) {
    if (!handle) return false;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return wrapper->controller->get_user_playlists();
}

bool spotify_controller_get_playlist_tracks(spotify_controller_handle_t handle, const char* playlist_id) {
    if (!handle || !playlist_id) return false;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return wrapper->controller->get_playlist_tracks(playlist_id);
}

bool spotify_controller_search_tracks(spotify_controller_handle_t handle, const char* query, int limit) {
    if (!handle || !query) return false;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return wrapper->controller->search_tracks(query, limit);
}

bool spotify_controller_get_playback_state(spotify_controller_handle_t handle) {
    if (!handle) return false;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return wrapper->controller->get_current_playback_state();
}

bool spotify_controller_get_devices(spotify_controller_handle_t handle) {
    if (!handle) return false;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return wrapper->controller->get_available_devices();
}

bool spotify_controller_cast_to_chromecast(spotify_controller_handle_t handle,
                                          const char* chromecast_ip,
                                          const char* track_uri) {
    if (!handle || !chromecast_ip || !track_uri) return false;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return wrapper->controller->cast_to_chromecast(chromecast_ip, track_uri);
}

// Callback setters
void spotify_controller_set_auth_state_callback(spotify_controller_handle_t handle,
                                               spotify_auth_state_callback_t callback) {
    if (!handle) return;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    wrapper->auth_state_callback = callback;
}

void spotify_controller_set_connection_state_callback(spotify_controller_handle_t handle,
                                                     spotify_connection_state_callback_t callback) {
    if (!handle) return;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    wrapper->connection_state_callback = callback;
}

void spotify_controller_set_playback_state_callback(spotify_controller_handle_t handle,
                                                   spotify_playback_state_callback_t callback) {
    if (!handle) return;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    wrapper->playback_state_callback = callback;
}

void spotify_controller_set_playlists_callback(spotify_controller_handle_t handle,
                                              spotify_playlists_callback_t callback) {
    if (!handle) return;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    wrapper->playlists_callback = callback;
}

void spotify_controller_set_tracks_callback(spotify_controller_handle_t handle,
                                           spotify_tracks_callback_t callback) {
    if (!handle) return;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    wrapper->tracks_callback = callback;
}

void spotify_controller_set_devices_callback(spotify_controller_handle_t handle,
                                            spotify_devices_callback_t callback) {
    if (!handle) return;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    wrapper->devices_callback = callback;
}

void spotify_controller_set_error_callback(spotify_controller_handle_t handle,
                                          spotify_error_callback_t callback) {
    if (!handle) return;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    wrapper->error_callback = callback;
}

// State getters
spotify_auth_state_t spotify_controller_get_auth_state(spotify_controller_handle_t handle) {
    if (!handle) return SPOTIFY_AUTH_ERROR_STATE;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return convert_auth_state(wrapper->controller->get_auth_state());
}

spotify_connection_state_t spotify_controller_get_connection_state(spotify_controller_handle_t handle) {
    if (!handle) return SPOTIFY_CONNECTION_ERROR_STATE;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    return convert_connection_state(wrapper->controller->get_connection_state());
}

void spotify_controller_run_periodic_tasks(spotify_controller_handle_t handle) {
    if (!handle) return;

    spotify_controller_wrapper* wrapper = static_cast<spotify_controller_wrapper*>(handle);
    wrapper->controller->run_periodic_tasks();
}
