#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "device.h"
#include "epd_7in5_v2.h"
#include "esp_sleep.h"

#define BLINK_GPIO 48
#define PWR_GPIO   14

#define I2C_MASTER_SCL_IO 9
#define I2C_MASTER_SDA_IO 3
#define I2C_MASTER_FREQ_HZ 100000

#define W25Q128_HOST SPI2_HOST
#define W25Q128_CLK_GPIO 36
#define W25Q128_MISO_GPIO 47
#define W25Q128_MOSI_GPIO 35
#define W25Q128_CS_GPIO 21

extern uint8_t photo[];

static void i2c_scan(i2c_master_bus_handle_t bus_handle)
{
    printf("Scanning I2C bus...\n");
    for (uint8_t i = 1; i < 127; i++) {
        esp_err_t ret = i2c_master_probe(bus_handle, i, -1);
        if (ret == ESP_OK) {
            printf("Found device at address 0x%02X\n", i);
        }
    }
}

static void read_w25q128_id(void)
{
    printf("Reading W25Q128 JEDEC ID...\n");
    spi_bus_config_t buscfg = {
        .miso_io_num = W25Q128_MISO_GPIO,
        .mosi_io_num = W25Q128_MOSI_GPIO,
        .sclk_io_num = W25Q128_CLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4
    };
    spi_device_interface_config_t devcfg = {
        .command_bits = 8,
        .clock_speed_hz = 10 * 1000 * 1000, // 10 MHz
        .mode = 0,
        .spics_io_num = W25Q128_CS_GPIO,
        .queue_size = 1,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(W25Q128_HOST, &buscfg, SPI_DMA_CH_AUTO));
    spi_device_handle_t spi;
    ESP_ERROR_CHECK(spi_bus_add_device(W25Q128_HOST, &devcfg, &spi));

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 24; // JEDEC ID is 3 bytes
    t.cmd = 0x9F;
    t.flags = SPI_TRANS_USE_RXDATA;

    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));

    printf("JEDEC ID: %02X %02X %02X\n", t.rx_data[0], t.rx_data[1], t.rx_data[2]);

    memset(&t, 0, sizeof(t));
    t.cmd = 0xB9; // Deep Power-Down command
    t.length = 8; // Command is 8 bits

    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));

    printf("W25Q128 go to sleep\n");

    memset(&t, 0, sizeof(t));
    t.length = 24; // JEDEC ID is 3 bytes
    t.cmd = 0x9F;
    t.flags = SPI_TRANS_USE_RXDATA;

    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));
    printf("JEDEC ID after sleep: %02X %02X %02X\n", t.rx_data[0], t.rx_data[1], t.rx_data[2]);

    ESP_ERROR_CHECK(spi_bus_remove_device(spi));
    ESP_ERROR_CHECK(spi_bus_free(W25Q128_HOST));
}

void app_main(void)
{
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("hello world hardware v2.1\n");

    gpio_reset_pin(PWR_GPIO);
    gpio_set_direction(PWR_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PWR_GPIO, 1);
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BLINK_GPIO, 0);

    for (int i = 0; i < 3; i++) {
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    i2c_master_bus_config_t i2c_bus_config = {
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    i2c_scan(bus_handle);

    ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));

    read_w25q128_id();

    device_init();
    epd_7in5_v2_init();
    epd_7in5_v2_clear();
    DELAY_MS(500);
    printf("Showing image...");
    epd_7in5_v2_display(photo);
    DELAY_MS(3000);
    epd_7in5_v2_init();
    epd_7in5_v2_clear();
    printf("Display go to sleep");
    epd_7in5_v2_sleep();

    printf("Going to sleep for 60s\n");

    // Power off peripherals
    gpio_set_level(BLINK_GPIO, 1);
    gpio_set_level(PWR_GPIO, 0);

    esp_sleep_enable_timer_wakeup(60 * 1000000);
    esp_deep_sleep_start();
}
