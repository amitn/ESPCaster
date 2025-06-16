
idf_component_register(
    SRCS "test_async_discovery.cpp" "example_integration.cpp" "chromecast_discovery.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        "json"
        "protobuf-c"
        "mdns"
        "esp_wifi"
        "nvs_flash"
        "freertos"
        "esp_common"
        "log"
        "esp-tls"
        "chromecast_controller"
        "lvgl__lvgl"
)

# Add compiler flags for C++
target_compile_options(${COMPONENT_LIB} PRIVATE -std=c++17)
