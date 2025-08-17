#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "soc/rtc.h"

#define LED_GPIO                    3
#define I2C_MASTER_SCL_IO           12
#define I2C_MASTER_SDA_IO           11
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0

static const char *TAG = "SELF_TEST";

static void led_blink(int times, int interval_ms)
{
    for (int i = 0; i < times; i++) {
        gpio_set_level(LED_GPIO, 0); // LED ON
        vTaskDelay(pdMS_TO_TICKS(interval_ms / 2));
        gpio_set_level(LED_GPIO, 1); // LED OFF
        vTaskDelay(pdMS_TO_TICKS(interval_ms / 2));
    }
}

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

static void i2c_scan(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus...");
    uint8_t address;
    int devices_found = 0;
    for (address = 1; address < 127; address++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Found device at address 0x%02X", address);
            devices_found++;
        }
    }
    if (devices_found == 0) {
        ESP_LOGI(TAG, "No I2C devices found");
    }
}

void app_main(void)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);
    
    // 1. Initial Delay
    ESP_LOGI(TAG, "Initial delay for 5 seconds...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    // 2. Hello World & LED Blink
    ESP_LOGI(TAG, "Hello World!");
    ESP_LOGI(TAG, "Blinking LED 5 times...");
    led_blink(5, 500);

    // 3. I2C Scan
    ESP_ERROR_CHECK(i2c_master_init());
    i2c_scan();
    ESP_ERROR_CHECK(i2c_driver_delete(I2C_MASTER_NUM));

    // 4. Countdown
    ESP_LOGI(TAG, "LED ON for 30 seconds countdown...");
    gpio_set_level(LED_GPIO, 0); // LED ON
    for (int i = 30; i > 0; i--) {
        ESP_LOGI(TAG, "Countdown: %d", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // 5. Deep Sleep
    ESP_LOGI(TAG, "Entering deep sleep for 60 seconds...");
    gpio_set_level(LED_GPIO, 1); // LED OFF

    // Configure RTC clock source for deep sleep
    rtc_clk_slow_freq_set(RTC_SLOW_FREQ_32K_XTAL);

    esp_sleep_enable_timer_wakeup(60 * 1000000); // 60 seconds
    esp_deep_sleep_start();
}
