#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "ds3231.h"

static const char *TAG = "DS3231_DEEPSLEEP_DEMO";

static i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t ds3231_handle;

// --- BCD (Binary-Coded Decimal) Conversion Helpers ---
// DS3231 使用 BCD 格式儲存時間，需要轉換
static uint8_t bcd2dec(uint8_t val) {
    return (val >> 4) * 10 + (val & 0x0F);
}

static uint8_t dec2bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

// --- I2C Initialization ---
esp_err_t rtc_i2c_master_init(void) {
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t ret = i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle);
    if (ret != ESP_OK) return ret;

    // Configure DS3231_INT_PIN as input with pull-up
    gpio_config_t io_conf_int = {};
    io_conf_int.pin_bit_mask = (1ULL << DS3231_INT_PIN);
    io_conf_int.mode = GPIO_MODE_INPUT;
    io_conf_int.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf_int.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf_int);

    

    i2c_device_config_t ds3231_dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DS3231_I2C_ADDRESS,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ret = i2c_master_bus_add_device(i2c_bus_handle, &ds3231_dev_config, &ds3231_handle);
    return ret;
}

// --- DS3231 Functions ---
esp_err_t rtc_ds3231_get_time(struct tm *timeinfo) {
    uint8_t data[7];
    uint8_t reg_addr = DS3231_REG_TIME;
    esp_err_t ret = i2c_master_transmit_receive(ds3231_handle, &reg_addr, 1, data, 7, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        return ret;
    }
    timeinfo->tm_sec = bcd2dec(data[0]);
    timeinfo->tm_min = bcd2dec(data[1]);
    timeinfo->tm_hour = bcd2dec(data[2]);
    timeinfo->tm_wday = bcd2dec(data[3]) - 1; // tm_wday is 0-6, DS3231 is 1-7
    timeinfo->tm_mday = bcd2dec(data[4]);
    timeinfo->tm_mon = bcd2dec(data[5] & 0x1F) - 1; // tm_mon is 0-11
    timeinfo->tm_year = bcd2dec(data[6]) + 100; // tm_year is years since 1900
    return ESP_OK;
}

esp_err_t rtc_ds3231_set_time(const struct tm *timeinfo) {
    uint8_t data[8];
    data[0] = DS3231_REG_TIME; // Start register address
    data[1] = dec2bcd(timeinfo->tm_sec);
    data[2] = dec2bcd(timeinfo->tm_min);
    data[3] = dec2bcd(timeinfo->tm_hour);
    data[4] = dec2bcd(timeinfo->tm_wday + 1); // DS3231 is 1-7
    data[5] = dec2bcd(timeinfo->tm_mday);
    data[6] = dec2bcd(timeinfo->tm_mon + 1); // DS3231 is 1-12
    data[7] = dec2bcd(timeinfo->tm_year - 100);
    return i2c_master_transmit(ds3231_handle, data, 8, pdMS_TO_TICKS(1000));
}

// 設定鬧鐘1，模式為：當秒、分、時、日都匹配時觸發
esp_err_t rtc_ds3231_set_alarm1(const struct tm *timeinfo) {
    uint8_t alarm_data[5];
    alarm_data[0] = DS3231_REG_ALARM1; // Start register for Alarm 1
    // A1M1 to A1M4 bits are 0, meaning alarm triggers when seconds, minutes, hours, and date/day match.
    alarm_data[1] = dec2bcd(timeinfo->tm_sec);   // Seconds
    alarm_data[2] = dec2bcd(timeinfo->tm_min);   // Minutes
    alarm_data[3] = dec2bcd(timeinfo->tm_hour);  // Hours
    alarm_data[4] = dec2bcd(timeinfo->tm_mday);  // Date of month
    
    esp_err_t ret = i2c_master_transmit(ds3231_handle, alarm_data, 5, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) return ret;

    // 現在啟用鬧鐘中斷
    uint8_t control_reg;
    uint8_t reg_addr = DS3231_REG_CONTROL;
    // 讀取目前的控制暫存器值
    ret = i2c_master_transmit_receive(ds3231_handle, &reg_addr, 1, &control_reg, 1, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) return ret;
    
    // 設定 INTCN=1 (啟用中斷輸出), A1IE=1 (啟用鬧鐘1中斷)
    control_reg |= (1 << 2); // INTCN = 1
    control_reg |= (1 << 0); // A1IE = 1

    uint8_t write_buf[2] = {DS3231_REG_CONTROL, control_reg};
    return i2c_master_transmit(ds3231_handle, write_buf, 2, pdMS_TO_TICKS(1000));
}

// 清除鬧鐘1觸發的旗標
esp_err_t rtc_ds3231_clear_alarm1_flag(void) {
    uint8_t status_reg;
    uint8_t reg_addr = DS3231_REG_STATUS;
    // 讀取狀態暫存器
    esp_err_t ret = i2c_master_transmit_receive(ds3231_handle, &reg_addr, 1, &status_reg, 1, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) return ret;
    ESP_LOGI(TAG, "DS3231 Status Register (before clear): 0x%02x", status_reg);

    // 清除 A1F (Alarm 1 Flag) 位 (寫 0 到該位)
    status_reg &= ~(1 << 0);
    ESP_LOGI(TAG, "DS3231 Status Register (after clear): 0x%02x", status_reg);

    uint8_t write_buf[2] = {DS3231_REG_STATUS, status_reg};
    return i2c_master_transmit(ds3231_handle, write_buf, 2, pdMS_TO_TICKS(1000));
}

