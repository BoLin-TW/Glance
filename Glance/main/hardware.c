#include "hardware.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#define I2C_MASTER_PORT    I2C_NUM_0
#define SPI_HOST           SPI2_HOST

static const char *TAG = "hardware";
static i2c_master_bus_handle_t bus_handle;

/**
 * @brief Initialize hardware components in a specific sequence.
 */
void hardware_init(void)
{
    ESP_LOGI(TAG, "Initializing hardware...");

    // 1. Init PWR and LED, then power on V33_2 rail
    ESP_LOGI(TAG, "Initializing power and LED...");
    gpio_config_t pwr_led_conf = {
        .pin_bit_mask = (1ULL << PIN_PWR_CTRL) | (1ULL << PIN_LED),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&pwr_led_conf);
    gpio_set_level(PIN_LED, 1);      // LED off (active low)
    gpio_set_level(PIN_PWR_CTRL, 1); // V33_2 power on
    ESP_LOGI(TAG, "V33_2 power rail is ON");
    vTaskDelay(100 / portTICK_PERIOD_MS); // Wait for peripherals to stabilize

    // 2. Set input pins
    ESP_LOGI(TAG, "Initializing input pins...");
    gpio_config_t in_conf = {
        .pin_bit_mask = (1ULL << PIN_BTN) | (1ULL << PIN_EPD_BSY) | (1ULL << PIN_I2C_ALRT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&in_conf);

    // 3. Init I2C
    ESP_LOGI(TAG, "Initializing I2C master...");
    i2c_master_bus_config_t i2c_bus_config = {
        .scl_io_num = PIN_I2C_SCL,
        .sda_io_num = PIN_I2C_SDA,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_PORT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));
    ESP_LOGI(TAG, "I2C master initialized.");

    // 4. Init EPD and Flash GPIOs
    ESP_LOGI(TAG, "Initializing EPD and Flash GPIOs...");
    gpio_config_t spi_gpio_conf = {
        .pin_bit_mask = (1ULL << PIN_EPD_RST) | (1ULL << PIN_EPD_DC) | (1ULL << PIN_EPD_CS) | (1ULL << PIN_FLS_CS),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&spi_gpio_conf);
    gpio_set_level(PIN_EPD_RST, 1);
    gpio_set_level(PIN_EPD_CS, 1);
    gpio_set_level(PIN_FLS_CS, 1);

    // 5. Init SPI
    ESP_LOGI(TAG, "Initializing SPI bus...");
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 8192
    };
    esp_err_t ret = spi_bus_initialize(SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPI bus initialized.");
    }
}

/**
 * @brief Controls the state of the on-board LED.
 * @param on true to turn the LED on, false to turn it off.
 */
void hardware_set_led(bool on)
{
    // The LED is active-low, so 'on' means level 0.
    gpio_set_level(PIN_LED, on ? 0 : 1);
}

/**
 * @brief De-initialize hardware components before entering deep sleep.
 */
void hardware_deinit(void)
{
    ESP_LOGI(TAG, "De-initializing hardware...");

    // De-init in reverse order
    ESP_LOGI(TAG, "De-initializing SPI bus...");
    spi_bus_free(SPI_HOST);

    ESP_LOGI(TAG, "De-initializing I2C master...");
    ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));

    ESP_LOGI(TAG, "Resetting GPIOs...");
    gpio_reset_pin(PIN_LED);
    gpio_reset_pin(PIN_BTN);
    gpio_reset_pin(PIN_EPD_BSY);
    gpio_reset_pin(PIN_I2C_ALRT);
    gpio_reset_pin(PIN_EPD_RST);
    gpio_reset_pin(PIN_EPD_DC);
    gpio_reset_pin(PIN_EPD_CS);
    gpio_reset_pin(PIN_FLS_CS);

    // Finally, power off the peripheral power rail
    gpio_set_level(PIN_PWR_CTRL, 0);
    ESP_LOGI(TAG, "V33_2 power rail is OFF");
    gpio_reset_pin(PIN_PWR_CTRL);
}