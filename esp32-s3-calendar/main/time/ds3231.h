#ifndef ESP32_S3_RTC_H
#define ESP32_S3_RTC_H

#include "esp_err.h"
#include <time.h>
#include "driver/i2c_master.h"

// --- I2C and DS3231 Configuration ---
#define I2C_MASTER_SCL_IO           0      // SCL 接腳
#define I2C_MASTER_SDA_IO           1      // SDA 接腳
#define I2C_MASTER_NUM              I2C_NUM_0 // 使用 I2C port 0
#define I2C_MASTER_FREQ_HZ          100000  // I2C 時脈頻率
#define I2C_MASTER_TX_BUF_DISABLE   0       // I2C master 不使用 buffer
#define I2C_MASTER_RX_BUF_DISABLE   0       // I2C master 不使用 buffer

#define DS3231_I2C_ADDRESS          0x68    // DS3231 的 I2C 位址

// --- Wakeup Pin Configuration ---
#define DS3231_INT_PIN              2      // 連接 DS3231 的 SQW/INT 腳位

// --- DS3231 Register Addresses ---
#define DS3231_REG_TIME             0x00
#define DS3231_REG_ALARM1           0x07
#define DS3231_REG_CONTROL          0x0E
#define DS3231_REG_STATUS           0x0F


// --- Public DS3231 Functions ---
esp_err_t rtc_i2c_master_init(void);
esp_err_t rtc_ds3231_get_time(struct tm *timeinfo);
esp_err_t rtc_ds3231_set_time(const struct tm *timeinfo);
esp_err_t rtc_ds3231_set_alarm1(const struct tm *timeinfo);
esp_err_t rtc_ds3231_clear_alarm1_flag(void);

#endif // ESP32_S3_RTC_H