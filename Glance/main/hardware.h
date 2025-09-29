#ifndef HARDWARE_H
#define HARDWARE_H

#include "esp_err.h"

// GPIO Definitions from GEMINI.md
#define GPIO_PWR        14
#define GPIO_LED        48
#define GPIO_BTN        1
#define GPIO_EPD_BSY    40
#define GPIO_EPD_RST    39
#define GPIO_EPD_DC     38
#define GPIO_EPD_CS     37
#define GPIO_FLS_CS     21
#define GPIO_I2C_SDA    3
#define GPIO_I2C_SCL    9
#define GPIO_I2C_ALRT   10

/**
 * @brief Initializes all hardware components of the device.
 *
 * This function should be called once at startup.
 * It configures GPIOs, and other peripherals.
 *
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
esp_err_t hardware_init(void);

#endif // HARDWARE_H
