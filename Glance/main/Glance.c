#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "hardware.h"
#include "wifi_manager.h"

/**
 * @brief Application states
 */
typedef enum {
    APP_STATE_INIT,
    APP_STATE_WIFI_CONNECT,
    APP_STATE_IDLE,
    APP_STATE_DEEPSLEEP,
    APP_STATE_ERROR,
} app_state_t;

// Set the initial state
static app_state_t current_state = APP_STATE_INIT;
static int idle_loops = 0;

void app_main(void)
{
    // A short delay to allow peripherals to power on before initialization.
    vTaskDelay(200 / portTICK_PERIOD_MS);
    while (1) {
        switch (current_state) {
            case APP_STATE_INIT:
                printf("Entering state: INIT\n");
                hardware_init();
                current_state = APP_STATE_WIFI_CONNECT;
                break;

            case APP_STATE_WIFI_CONNECT:
                printf("Entering state: WIFI_CONNECT\n");
                hardware_set_led(true); // Turn LED on while connecting
                if (wifi_connect()) {
                    display_hardware_init(); // Initialize display hardware after Wi-Fi
                    current_state = APP_STATE_IDLE;
                    idle_loops = 0; // Reset idle loop counter
                } else {
                    printf("Wi-Fi connection failed.\n");
                    current_state = APP_STATE_ERROR;
                }
                hardware_set_led(false); // Turn LED off
                break;

            case APP_STATE_IDLE:
                printf("Entering state: IDLE (%d/100)\n", idle_loops);
                hardware_set_led(idle_loops % 2 == 0); // Blink the LED
                if (++idle_loops >= 100) {
                    current_state = APP_STATE_DEEPSLEEP;
                }
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
        // A small delay to prevent the loop from spinning without yielding
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
