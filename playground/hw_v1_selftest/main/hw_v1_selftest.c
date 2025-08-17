#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "soc/rtc.h"

#define LED_GPIO                    3
#define BTN_GPIO                    13
#define I2C_MASTER_SCL_IO           12
#define I2C_MASTER_SDA_IO           11
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0

static const char *TAG = "SELF_TEST";

static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void button_task(void* arg)
{
    uint32_t io_num;
    static uint32_t last_press_time = 0;
    const uint32_t debounce_time_ms = 50;

    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if ((current_time - last_press_time) > debounce_time_ms) {
                ESP_LOGI(TAG, "Button on GPIO %ld pushed.", io_num);
                last_press_time = current_time;
            }
        }
    }
}


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
    // Turn on LED as soon as MCU boots up
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0); // LED ON

    // Button interrupt setup
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = (1ULL<<BTN_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_GPIO, gpio_isr_handler, (void*) BTN_GPIO);


    // 1. Initial Delay
    ESP_LOGI(TAG, "Device is AWAKE. Initial delay for 5 seconds...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    // 2. Hello World & LED Blink
    ESP_LOGI(TAG, "Hello World!");
    ESP_LOGI(TAG, "Blinking LED 5 times...");
    led_blink(5, 500); // This will leave the LED OFF

    // Turn LED back on for the rest of the awake time
    gpio_set_level(LED_GPIO, 0); // LED ON

    // 3. I2C Scan
    ESP_ERROR_CHECK(i2c_master_init());
    i2c_scan();
    ESP_ERROR_CHECK(i2c_driver_delete(I2C_MASTER_NUM));

    // 4. Countdown
    ESP_LOGI(TAG, "LED is ON for 30 seconds countdown...");
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
