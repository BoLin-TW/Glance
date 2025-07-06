#ifndef SNTP_TIME_H
#define SNTP_TIME_H

#include "esp_err.h"

/**
 * @brief Initializes the SNTP service and sets the local timezone.
 *
 * This function configures and starts the SNTP service to get network time.
 * It also sets the device's timezone to CST-8 (China/Taiwan Standard Time).
 * Call this once after a successful Wi-Fi connection.
 */
void sntp_time_init(void);

/**
 * @brief Waits for the SNTP service to synchronize the system time.
 *
 * @param timeout_sec The maximum time to wait for synchronization, in seconds.
 * @return
 *     - ESP_OK if time was successfully synchronized within the timeout.
 *     - ESP_ERR_TIMEOUT if the timeout was reached before synchronization.
 */
esp_err_t sntp_time_wait_for_sync(int timeout_sec);

#endif // SNTP_TIME_H
