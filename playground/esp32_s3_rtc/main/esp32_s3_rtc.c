#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_log.h"

static const char *TAG = "DS3231_DEEPSLEEP_DEMO";

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

// --- BCD (Binary-Coded Decimal) Conversion Helpers ---
// DS3231 使用 BCD 格式儲存時間，需要轉換
static uint8_t bcd2dec(uint8_t val) {
    return (val >> 4) * 10 + (val & 0x0F);
}

static uint8_t dec2bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

// --- I2C Initialization ---
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

// --- DS3231 Functions ---
static esp_err_t ds3231_get_time(struct tm *timeinfo) {
    uint8_t data[7];
    esp_err_t ret = i2c_master_write_read_device(I2C_MASTER_NUM, DS3231_I2C_ADDRESS,
                                                 (uint8_t[]){DS3231_REG_TIME}, 1,
                                                 data, 7, pdMS_TO_TICKS(1000));
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

static esp_err_t ds3231_set_time(const struct tm *timeinfo) {
    uint8_t data[8];
    data[0] = DS3231_REG_TIME; // Start register address
    data[1] = dec2bcd(timeinfo->tm_sec);
    data[2] = dec2bcd(timeinfo->tm_min);
    data[3] = dec2bcd(timeinfo->tm_hour);
    data[4] = dec2bcd(timeinfo->tm_wday + 1); // DS3231 is 1-7
    data[5] = dec2bcd(timeinfo->tm_mday);
    data[6] = dec2bcd(timeinfo->tm_mon + 1); // DS3231 is 1-12
    data[7] = dec2bcd(timeinfo->tm_year - 100);
    return i2c_master_write_to_device(I2C_MASTER_NUM, DS3231_I2C_ADDRESS, data, 8, pdMS_TO_TICKS(1000));
}

// 設定鬧鐘1，模式為：當秒、分、時、日都匹配時觸發
static esp_err_t ds3231_set_alarm1(const struct tm *timeinfo) {
    uint8_t alarm_data[5];
    alarm_data[0] = DS3231_REG_ALARM1; // Start register for Alarm 1
    // A1M1 to A1M4 bits are 0, meaning alarm triggers when seconds, minutes, hours, and date/day match.
    alarm_data[1] = dec2bcd(timeinfo->tm_sec);   // Seconds
    alarm_data[2] = dec2bcd(timeinfo->tm_min);   // Minutes
    alarm_data[3] = dec2bcd(timeinfo->tm_hour);  // Hours
    alarm_data[4] = dec2bcd(timeinfo->tm_mday);  // Date of month
    
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, DS3231_I2C_ADDRESS, alarm_data, sizeof(alarm_data), pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) return ret;

    // 現在啟用鬧鐘中斷
    uint8_t control_reg;
    // 讀取目前的控制暫存器值
    i2c_master_write_read_device(I2C_MASTER_NUM, DS3231_I2C_ADDRESS, (uint8_t[]){DS3231_REG_CONTROL}, 1, &control_reg, 1, pdMS_TO_TICKS(1000));
    
    // 設定 INTCN=1 (啟用中斷輸出), A1IE=1 (啟用鬧鐘1中斷)
    control_reg |= (1 << 2); // INTCN = 1
    control_reg |= (1 << 0); // A1IE = 1

    uint8_t write_buf[2] = {DS3231_REG_CONTROL, control_reg};
    return i2c_master_write_to_device(I2C_MASTER_NUM, DS3231_I2C_ADDRESS, write_buf, sizeof(write_buf), pdMS_TO_TICKS(1000));
}

// 清除鬧鐘1觸發的旗標
static esp_err_t ds3231_clear_alarm1_flag(void) {
    uint8_t status_reg;
    // 讀取狀態暫存器
    i2c_master_write_read_device(I2C_MASTER_NUM, DS3231_I2C_ADDRESS, (uint8_t[]){DS3231_REG_STATUS}, 1, &status_reg, 1, pdMS_TO_TICKS(1000));

    // 清除 A1F (Alarm 1 Flag) 位 (寫 0 到該位)
    status_reg &= ~(1 << 0);

    uint8_t write_buf[2] = {DS3231_REG_STATUS, status_reg};
    return i2c_master_write_to_device(I2C_MASTER_NUM, DS3231_I2C_ADDRESS, write_buf, sizeof(write_buf), pdMS_TO_TICKS(1000));
}

void app_main(void) {
    ESP_LOGI(TAG, "DS3231 + Deep Sleep Demo Starting...");

    // 初始化 I2C
    ESP_ERROR_CHECK(i2c_master_init());

    // 檢查喚醒原因
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_EXT0) {
        ESP_LOGI(TAG, "Normal boot (not from deep sleep alarm). Setting up initial time.");
        // 首次啟動，設定 DS3231 時間 (例如，設定為編譯時間)
        struct tm timeinfo = {
            .tm_year = 2025 - 1900, // Year - 1900
            .tm_mon = 6 - 1,       // Month (0-11) -> June
            .tm_mday = 15,         // Day
            .tm_hour = 20,         // Hour (24h)
            .tm_min = 20,          // Minute
            .tm_sec = 0            // Second
        };
        // 計算星期幾 (mktime 會自動計算)
        mktime(&timeinfo);
        esp_err_t set_time_ret = ds3231_set_time(&timeinfo);
        if (set_time_ret == ESP_OK) {
             ESP_LOGI(TAG, "Initial time set successfully.");
        } else {
             ESP_LOGE(TAG, "Failed to set initial time: %s", esp_err_to_name(set_time_ret));
        }

    } else {
        ESP_LOGI(TAG, "Woken up from deep sleep by DS3231 alarm.");
        // 從深度睡眠喚醒，必須清除鬧鐘旗標
        esp_err_t clear_flag_ret = ds3231_clear_alarm1_flag();
        if (clear_flag_ret == ESP_OK) {
            ESP_LOGI(TAG, "DS3231 Alarm 1 flag cleared.");
        } else {
             ESP_LOGE(TAG, "Failed to clear DS3231 alarm flag: %s", esp_err_to_name(clear_flag_ret));
        }
    }

    // --- 通用流程：讀取時間、設定下一個鬧鐘、進入睡眠 ---

    // 讀取並打印當前時間
    struct tm current_time;
    if (ds3231_get_time(&current_time) == ESP_OK) {
        char buffer[50];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &current_time);
        ESP_LOGI(TAG, "Current time from DS3231: %s", buffer);
    } else {
        ESP_LOGE(TAG, "Failed to get time from DS3231.");
        // 發生錯誤，延時後重啟
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }

    // 計算並設定下一個鬧鐘時間 (例如：20 秒後)
    time_t now_ts = mktime(&current_time);
    now_ts += 20; // 增加 20 秒
    struct tm next_alarm_time;
    localtime_r(&now_ts, &next_alarm_time); // 將 time_t 轉回 struct tm

    if (ds3231_set_alarm1(&next_alarm_time) == ESP_OK) {
         char buffer[50];
         strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &next_alarm_time);
         ESP_LOGI(TAG, "Next alarm set for: %s", buffer);
    } else {
         ESP_LOGE(TAG, "Failed to set next alarm.");
    }
    
    // 配置外部喚醒源 (EXT0)
    ESP_LOGI(TAG, "Configuring EXT0 wakeup on GPIO %d", DS3231_INT_PIN);
    // DS3231 的 INT 腳位在觸發時會被拉到 LOW，所以設定為低電平觸發
    esp_sleep_enable_ext0_wakeup(DS3231_INT_PIN, 0);

    // 進入深度睡眠
    ESP_LOGI(TAG, "Entering deep sleep now...");
    // 延遲一小段時間確保日誌能完整打印出去
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_deep_sleep_start();
}