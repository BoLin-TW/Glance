#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

/**
 * @brief Connects to the Wi-Fi network using credentials from credentials.h
 *
 * This is a blocking function.
 *
 * @return true if connection is successful, false otherwise.
 */
bool wifi_connect(void);

/**
 * @brief Disconnects from the Wi-Fi network and de-initializes the Wi-Fi driver.
 */
void wifi_disconnect(void);

#endif // WIFI_MANAGER_H
