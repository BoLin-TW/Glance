#include "nvs_flash.h"
#include "esp_log.h"
#include "wifi/wifi_manager.h"
#include "time/sntp_time.h"
#include "ics/ics_parser.h"
#include "epaper/device.h"
#include "epaper/epd_7in5_v2.h"
#include "time/ds3231.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "ics_config.h"

// Define a simple image for testing
static uint8_t image[EPD_7IN5_V2_WIDTH * EPD_7IN5_V2_HEIGHT / 8];

static void draw_test_pattern() {
    // Fill the image buffer with a simple pattern (e.g., a checkerboard)
    for (int y = 0; y < EPD_7IN5_V2_HEIGHT; y++) {
        for (int x = 0; x < EPD_7IN5_V2_WIDTH / 8; x++) {
            if ((x + y) % 2 == 0) {
                image[y * (EPD_7IN5_V2_WIDTH / 8) + x] = 0xAA; // Black and white pattern
            } else {
                image[y * (EPD_7IN5_V2_WIDTH / 8) + x] = 0x55;
            }
        }
    }
}


static const char *TAG = "APP_MAIN";

static void print_upcoming_events(int count) {
    calendar_event_t *events;
    int event_count = ics_parser_get_events(&events);

    if (event_count == 0) {
        ESP_LOGI(TAG, "No upcoming events found.");
        return;
    }

    ESP_LOGI(TAG, "--- Upcoming Events (Max %d) ---", count);
    int print_count = (event_count < count) ? event_count : count;

    tzset();
    time_t now_ts;
    time(&now_ts);
    struct tm timeinfo_local;
    localtime_r(&now_ts, &timeinfo_local);
    char current_time_buf[64];
    strftime(current_time_buf, sizeof(current_time_buf), "%Y-%m-%d %H:%M:%S %Z", &timeinfo_local);
    ESP_LOGI(TAG, "Current Time (time_t: %lld): %s", (long long)now_ts, current_time_buf);

    for (int i = 0; i < print_count; i++) {
        struct tm *local_tm = localtime(&events[i].start_time);
        char time_buf[64];
        if (local_tm) {
             strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S %Z", local_tm);
        } else {
             snprintf(time_buf, sizeof(time_buf), "Invalid Time");
        }
        ESP_LOGI(TAG, "%d: %s - %s", i + 1, time_buf, events[i].summary);
    }
    ESP_LOGI(TAG, "-----------------------------");
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Application");

    // 1. Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Initialize RTC
    ESP_LOGI(TAG, "Initializing I2C and DS3231 RTC...");
    ESP_ERROR_CHECK(rtc_i2c_master_init());

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_EXT0) {
        ESP_LOGI(TAG, "Normal boot (not from deep sleep alarm). Setting up initial time.");
        struct tm timeinfo = {
            .tm_year = 2025 - 1900, // Year - 1900
            .tm_mon = 7 - 1,       // Month (0-11) -> July
            .tm_mday = 5,         // Day
            .tm_hour = 12,         // Hour (24h)
            .tm_min = 0,          // Minute
            .tm_sec = 0            // Second
        };
        mktime(&timeinfo);
        ESP_ERROR_CHECK(rtc_ds3231_set_time(&timeinfo));
        ESP_LOGI(TAG, "Initial time set successfully.");
    } else {
        ESP_LOGI(TAG, "Woken up from deep sleep by DS3231 alarm.");
        ESP_ERROR_CHECK(rtc_ds3231_clear_alarm1_flag());
        ESP_LOGI(TAG, "DS3231 Alarm 1 flag cleared.");
    }

    // 3. Initialize E-Paper Display
    // ESP_LOGI(TAG, "Initializing E-Paper Display...");
    // device_init();
    // epd_7in5_v2_init();
    // epd_7in5_v2_clear();
    // ESP_LOGI(TAG, "E-Paper Display Initialized.");

    // Draw a test pattern and display it
    // draw_test_pattern();
    // epd_7in5_v2_display(image);
    // ESP_LOGI(TAG, "Test pattern displayed on E-Paper.");

    // 4. Initialize Wi-Fi Manager and connect
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(wifi_manager_start());
    ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
    esp_err_t wifi_status = wifi_manager_wait_for_ip(portMAX_DELAY);

    if (wifi_status == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi Connected. Initializing SNTP...");
        sntp_time_init();
        if (sntp_time_wait_for_sync(30) == ESP_OK) {
            ESP_LOGI(TAG, "Time synchronized with SNTP. Setting DS3231 RTC.");
            time_t now_sntp;
            struct tm timeinfo_sntp;
            time(&now_sntp);
            localtime_r(&now_sntp, &timeinfo_sntp);
            ESP_ERROR_CHECK(rtc_ds3231_set_time(&timeinfo_sntp));
            ESP_LOGI(TAG, "DS3231 RTC set to SNTP time.");
        } else {
            ESP_LOGE(TAG, "Failed to sync time with SNTP. DS3231 RTC may be inaccurate.");
        }

        ESP_LOGI(TAG, "Fetching and parsing ICS data...");
        ics_parser_fetch_and_parse(ICS_URL);
        print_upcoming_events(10);

        ESP_LOGI(TAG, "Entering deep sleep mode.");

        // Get current time from DS3231
        struct tm current_time;
        ESP_ERROR_CHECK(rtc_ds3231_get_time(&current_time));
        time_t now_ts = mktime(&current_time);

        // Set alarm for 1 minute later
        now_ts += 60; // Add 60 seconds
        struct tm next_alarm_time;
        localtime_r(&now_ts, &next_alarm_time);

        ESP_ERROR_CHECK(rtc_ds3231_set_alarm1(&next_alarm_time));
        char buffer[50];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &next_alarm_time);
        ESP_LOGI(TAG, "Next alarm set for: %s", buffer);

        // Configure EXT0 wakeup
        ESP_LOGI(TAG, "Configuring EXT0 wakeup on GPIO %d", DS3231_INT_PIN);
        esp_sleep_enable_ext0_wakeup(DS3231_INT_PIN, 0); // DS3231 INT pin goes low on alarm

        // Enter deep sleep
        ESP_LOGI(TAG, "Entering deep sleep now...");
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to allow logs to flush and DS3231 INT pin to go high
        esp_deep_sleep_start();

    } else {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi after all attempts.");
        ESP_LOGE(TAG, "Halting application.");
        vTaskDelay(portMAX_DELAY); // Halt
    }
}

