#include "hardware.h"
#include "driver/gpio.h"

esp_err_t hardware_init(void)
{
    // Configure power control pin
    gpio_config_t pwr_conf = {
        .pin_bit_mask = (1ULL << GPIO_PWR),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&pwr_conf));

    // Configure LED pin
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << GPIO_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led_conf));

    // Configure button pin
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << GPIO_BTN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE, // Interrupt can be enabled later
    };
    ESP_ERROR_CHECK(gpio_config(&btn_conf));

    // E-Paper and Flash GPIOs will be configured by their respective drivers.
    // For now, we can leave them unconfigured or set them to a default state if needed.

    // I2C pins will be configured by the I2C driver.

    printf("Hardware initialized\n");

    return ESP_OK;
}
