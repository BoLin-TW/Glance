#include "sntp_time.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sntp.h"

static const char *TAG = "SNTP_TIME";

// --- SNTP Synchronization Status ---
static bool sntp_synchronized = false;

// --- SNTP Time Sync Callback ---
static void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "SNTP time synchronized: %ld seconds", tv->tv_sec);
    sntp_synchronized = true;
}

// --- Public Functions ---
void sntp_time_init(void) {
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    // Set Timezone to CST (UTC+8)
    setenv("TZ", "CST-8", 1);
    tzset();
    ESP_LOGI(TAG, "Timezone set to CST-8");
}

esp_err_t sntp_time_wait_for_sync(int timeout_sec) {
    int wait_count = 0;
    while (!sntp_synchronized && wait_count < timeout_sec) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        wait_count++;
    }
    if (!sntp_synchronized) {
        ESP_LOGE(TAG, "SNTP time synchronization failed within %d seconds.", timeout_sec);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}
