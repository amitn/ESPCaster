
idf_component_register(
    SRCS
    "chromecast_controller.cpp"
    "chromecast_protobuf/cast_channel.pb-c.c"
    "chromecast_protobuf/authority_keys.pb-c.c"
    "chromecast_protobuf/logging.pb-c.c"
    INCLUDE_DIRS "."
    REQUIRES
        "json"
        "protobuf-c"
        "esp_wifi"
        "nvs_flash"
        "freertos"
        "esp_common"
        "log"
        "esp-tls"
        "mbedtls"
)

# Add compiler flags for C++
# target_compile_options(${COMPONENT_LIB} PRIVATE -std=c++17)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wmissing-field-initializers)
