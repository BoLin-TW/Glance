#include "hardware.h"
#include <stdio.h>
#include "driver/gpio.h"

/**
 * @brief Initialize hardware components.
 *
 * This function powers on the peripheral power rail.
 * TODO: Add initialization for other peripherals like display, sensors, etc.
 */
void hardware_init(void)
{
    printf("Initializing hardware...\n");
    // Configure the power control pin as an output
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_PWR_CTRL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Turn on the V33_2 power rail for peripherals
    gpio_set_level(PIN_PWR_CTRL, 1);
    printf("V33_2 power rail is ON\n");
}

/**
 * @brief De-initialize hardware components before entering deep sleep.
 *
 * This function powers off the peripheral power rail.
 * TODO: Add de-initialization for other peripherals if needed.
 */
void hardware_deinit(void)
{
    printf("De-initializing hardware...\n");
    // Turn off the V33_2 power rail for peripherals
    gpio_set_level(PIN_PWR_CTRL, 0);
    printf("V33_2 power rail is OFF\n");
}