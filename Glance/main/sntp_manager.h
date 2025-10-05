#ifndef SNTP_MANAGER_H
#define SNTP_MANAGER_H

#include <stdbool.h>

/**
 * @brief Synchronizes the system time with an NTP server.
 *
 * This is a blocking function.
 * It assumes that Wi-Fi is already connected.
 *
 * @return true if time is successfully synchronized, false otherwise.
 */
bool glance_sntp_sync_time(void);

#endif // SNTP_MANAGER_H
