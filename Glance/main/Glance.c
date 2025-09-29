#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "hardware.h"

/**
 * @brief Application states
 */
typedef enum {
    APP_STATE_INIT,
    APP_STATE_IDLE,
    APP_STATE_DEEPSLEEP,
} app_state_t;

// Set the initial state
static app_state_t current_state = APP_STATE_INIT;
static int idle_loops = 0;

void app_main(void)
{
    while (1) {
        switch (current_state) {
            case APP_STATE_INIT:
                printf("Entering state: INIT\n");
                hardware_init();
                // Transition to the next state
                current_state = APP_STATE_IDLE;
                idle_loops = 0; // Reset idle loop counter
                break;

            case APP_STATE_IDLE:
                printf("Entering state: IDLE (%d/100)\n", idle_loops);
                if (++idle_loops >= 100) {
                    current_state = APP_STATE_DEEPSLEEP;
                }
                break;

            case APP_STATE_DEEPSLEEP:
                printf("Entering state: DEEPSLEEP\n");
                hardware_deinit();
                
                printf("Entering deep sleep now.\n");
                // The device will restart after waking up from deep sleep.
                esp_deep_sleep_start();
                // The code below this line will not be executed.
                break;
            
default:
                printf("Unknown state, resetting to INIT\n");
                current_state = APP_STATE_INIT;
                break;
        }
        // A small delay to prevent the loop from spinning without yielding
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
