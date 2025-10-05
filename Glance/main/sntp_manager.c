#include "sntp_manager.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <time.h>

static const char *TAG = "sntp_manager";

static SemaphoreHandle_t s_sync_sem;

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronization event received.");
    xSemaphoreGive(s_sync_sem);
}

bool glance_sntp_sync_time(void)
{
    s_sync_sem = xSemaphoreCreateBinary();
    if (s_sync_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return false;
    }

    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    ESP_LOGI(TAG, "Waiting for time synchronization...");
    
    if (xSemaphoreTake(s_sync_sem, pdMS_TO_TICKS(30000)) == pdTRUE) {
        time_t now;
        struct tm timeinfo;
        char strftime_buf[64];

        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
        
        esp_sntp_stop();
        vSemaphoreDelete(s_sync_sem);
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to synchronize time within 30 seconds");
        esp_sntp_stop();
        vSemaphoreDelete(s_sync_sem);
        return false;
    }
}
