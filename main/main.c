#include "Display_SPD2010.h"
#include "PCF85063.h"
#include "QMI8658.h"
#include "SD_MMC.h"
#include "Wireless.h"
#include "TCA9554PWR.h"
#include "LVGL_Example.h"
#include "BAT_Driver.h"
#include "PWR_Key.h"
#include "PCM5101.h"
#include "MIC_Speech.h"

#include "esp_cast.h"

// Uncomment to enable Spotify integration testing
// #define ENABLE_SPOTIFY_TESTS

#ifdef ENABLE_SPOTIFY_TESTS
extern void start_spotify_integration_tests(void);
#endif

void Driver_Loop(void *parameter)
{
    // Wireless_Init();
    esp_cast_wifi_init_sta();
    while(1)
    {
        QMI8658_Loop();
        PCF85063_Loop();
        BAT_Get_Volts();
        PWR_Loop();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}
void Driver_Init(void)
{
    PWR_Init();
    BAT_Init();
    I2C_Init();
    EXIO_Init();                    // Example Initialize EXIO
    Flash_Searching();
    PCF85063_Init();
    QMI8658_Init();
    xTaskCreatePinnedToCore(
        Driver_Loop, 
        "Other Driver task",
        4096, 
        NULL, 
        3, 
        NULL, 
        0);
}
void app_main(void)
{
    Driver_Init();

    // SD_Init();
    LCD_Init();
    // Audio_Init();
    // MIC_Speech_init();
    // Play_Music("/sdcard","AAA.mp3");
    LVGL_Init();   // returns the screen object

// /********************* Demo *********************/
    // Lvgl_Example1();
    esp_cast_gui_init();

    // Test default WiFi functionality (uncomment to test)
    // esp_cast_test_default_wifi();

    // Initialize Spotify integration (uncomment and configure to enable)
    /*
    bool spotify_success = esp_cast_spotify_init(
        "your_spotify_client_id",
        "your_spotify_client_secret",  // Can be NULL for PKCE flow
        "http://localhost:8888/callback"
    );
    if (spotify_success) {
        ESP_LOGI("MAIN", "Spotify integration initialized successfully");
    } else {
        ESP_LOGE("MAIN", "Failed to initialize Spotify integration");
    }
    */

#ifdef ENABLE_SPOTIFY_TESTS
    // Start Spotify integration tests
    start_spotify_integration_tests();
#endif

    // lv_demo_widgets();
    // lv_demo_keypad_encoder();
    // lv_demo_benchmark();
    // lv_demo_stress();
    // lv_demo_music();

    while (1) {
        // raise the task priority of LVGL and/or reduce the handler period can improve the performance
        vTaskDelay(pdMS_TO_TICKS(10));
        // The task running lv_timer_handler should have lower priority than that running `lv_tick_inc`
        lv_timer_handler();
        esp_cast_loop();
    }
}






