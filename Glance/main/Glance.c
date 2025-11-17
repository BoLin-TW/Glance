#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "hardware.h"
#include "wifi_manager.h"
#include "sntp_manager.h"
#include "esp_http_client.h"

/**
 * @brief Application states
 */
typedef enum {
    APP_STATE_INIT,
    APP_STATE_WIFI_CONNECT,
    APP_STATE_SYNC_TIMEZONE_START,
    APP_STATE_SYNC_TIMEZONE_WAIT,
    APP_STATE_SYNC_TIME,
    APP_STATE_IDLE,
    APP_STATE_DEEPSLEEP,
    APP_STATE_ERROR,
} app_state_t;

// App event group
static EventGroupHandle_t app_event_group;
#define TIMEZONE_SYNC_SUCCESS_BIT BIT0
#define TIMEZONE_SYNC_FAIL_BIT    BIT1

// Set the initial state
static app_state_t current_state = APP_STATE_INIT;
static int idle_loops = 0;

esp_err_t _http_event_handler_print_raw(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            break;
        case HTTP_EVENT_ON_CONNECTED:
            break;
        case HTTP_EVENT_HEADER_SENT:
            break;
        case HTTP_EVENT_ON_HEADER:
            break;
        case HTTP_EVENT_ON_DATA:
            printf("RAW_DATA: %.*s\n", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_FINISH:
            break;
        case HTTP_EVENT_DISCONNECTED:
            break;
        case HTTP_EVENT_REDIRECT:
            break;
        default:
            break;
    }
    return ESP_OK;
}

bool glance_timezone_sync(void)
{
    printf("glance_timezone_sync: Starting...\n");

    esp_http_client_config_t config = {
        .url = "http://ip-api.com/json",
        .event_handler = _http_event_handler_print_raw,
    };

    printf("glance_timezone_sync: Initializing HTTP client...\n");
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        printf("glance_timezone_sync: Failed to initialize HTTP client\n");
        return false;
    }

    printf("glance_timezone_sync: Performing HTTP GET request...\n");
    esp_err_t err = esp_http_client_perform(client);
    printf("glance_timezone_sync: HTTP GET request finished.\n");

    bool success = false;
    if (err == ESP_OK) {
        printf("HTTP GET Status = %d, content_length = %lld\n",
                (int)esp_http_client_get_status_code(client),
                (long long)esp_http_client_get_content_length(client));
        if (esp_http_client_get_status_code(client) == 200) {
            success = true;
        }
    } else {
        printf("HTTP GET request failed: %s\n", esp_err_to_name(err));
    }

    printf("glance_timezone_sync: Cleaning up HTTP client...\n");
    esp_http_client_cleanup(client);
    printf("glance_timezone_sync: Finished.\n");
    return success;
}

void timezone_sync_task(void *pvParameters)
{
    if (glance_timezone_sync()) {
        xEventGroupSetBits(app_event_group, TIMEZONE_SYNC_SUCCESS_BIT);
    } else {
        xEventGroupSetBits(app_event_group, TIMEZONE_SYNC_FAIL_BIT);
    }
    vTaskDelete(NULL); // Delete task when done
}

void app_main(void)
{
    // A short delay to allow peripherals to power on before initialization.
    vTaskDelay(200 / portTICK_PERIOD_MS);
    while (1) {
        switch (current_state) {
            case APP_STATE_INIT:
                printf("Entering state: INIT\n");
                app_event_group = xEventGroupCreate();
                hardware_init();
                current_state = APP_STATE_WIFI_CONNECT;
                break;

            case APP_STATE_WIFI_CONNECT:
                printf("Entering state: WIFI_CONNECT\n");
                hardware_set_led(true); // Turn LED on while connecting
                if (wifi_connect()) {
                    display_hardware_init(); // Initialize display hardware after Wi-Fi
                    current_state = APP_STATE_SYNC_TIMEZONE_START;
                } else {
                    printf("Wi-Fi connection failed.\n");
                    current_state = APP_STATE_ERROR;
                }
                hardware_set_led(false); // Turn LED off
                break;

            case APP_STATE_SYNC_TIMEZONE_START:
                printf("Entering state: SYNC_TIMEZONE_START\n");
                xTaskCreate(&timezone_sync_task, "timezone_sync_task", 4096, NULL, 5, NULL);
                current_state = APP_STATE_SYNC_TIMEZONE_WAIT;
                idle_loops = 0; // Reset counter for the wait state
                break;

            case APP_STATE_SYNC_TIMEZONE_WAIT:
                if (idle_loops == 0) {
                    printf("Entering state: SYNC_TIMEZONE_WAIT\n");
                }
                
                EventBits_t bits = xEventGroupWaitBits(app_event_group,
                    TIMEZONE_SYNC_SUCCESS_BIT | TIMEZONE_SYNC_FAIL_BIT,
                    pdTRUE,  // Clear bits on exit
                    pdFALSE, // Wait for any bit
                    10 / portTICK_PERIOD_MS); // Use a very short timeout

                if (bits & TIMEZONE_SYNC_SUCCESS_BIT) {
                    printf("Timezone sync successful.\n");
                    current_state = APP_STATE_SYNC_TIME;
                    hardware_set_led(false); // Turn off LED
                } else if (bits & TIMEZONE_SYNC_FAIL_BIT) {
                    printf("Timezone sync failed.\n");
                    current_state = APP_STATE_ERROR;
                    hardware_set_led(false); // Turn off LED
                } else {
                    // Timeout, still waiting. The state remains APP_STATE_SYNC_TIMEZONE_WAIT.
                    hardware_set_led(idle_loops++ % 2); // Blink the LED
                    vTaskDelay(200 / portTICK_PERIOD_MS); // Yield and wait a bit
                }
                break;

            case APP_STATE_SYNC_TIME:
                printf("Entering state: SYNC_TIME\n");
                hardware_set_led(true); // Turn LED on while syncing
                if (glance_sntp_sync_time()) {
                    current_state = APP_STATE_IDLE;
                    idle_loops = 0; // Reset idle loop counter
                } else {
                    printf("Time synchronization failed.\n");
                    current_state = APP_STATE_ERROR;
                }
                hardware_set_led(false); // Turn LED off
                break;

            case APP_STATE_IDLE:
                if (idle_loops == 0) {
                    printf("Entering state: IDLE\n");
                }

                // Get current time
                time_t now;
                struct tm timeinfo;
                time(&now);
                localtime_r(&now, &timeinfo);

                // Format and print time
                char strftime_buf[64];
                strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
                printf("Current time: %s\n", strftime_buf);

                hardware_set_led(idle_loops % 2 == 0); // Blink the LED

                if (++idle_loops >= 10) { // Run for 10 seconds
                    current_state = APP_STATE_DEEPSLEEP;
                }
                
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                break;

            case APP_STATE_DEEPSLEEP:
                printf("Entering state: DEEPSLEEP\n");
                wifi_disconnect();
                hardware_deinit();
                
                printf("Entering deep sleep now.\n");
                esp_deep_sleep_start();
                break;
            
default:
            case APP_STATE_ERROR:
                printf("Entering state: ERROR\n");
                // Stay in this state and blink LED rapidly to indicate error
                hardware_set_led(true);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                hardware_set_led(false);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                break;
        }
    }
}