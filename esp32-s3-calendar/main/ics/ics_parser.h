#ifndef ICS_PARSER_H
#define ICS_PARSER_H

#include "esp_err.h"
#include <time.h>

// --- Constants ---
#define MAX_EVENTS              50   // Maximum number of future events to handle
#define MAX_SUMMARY_LEN         100  // Maximum length of an event summary

// --- Event Structure ---
typedef struct {
    time_t start_time;                // Event start time (UTC time_t)
    char summary[MAX_SUMMARY_LEN]; // Event summary
} calendar_event_t;

/**
 * @brief Fetches and parses an ICS file from the given URL.
 *
 * This function initializes an HTTP client, performs a GET request, and parses
 * the ICS data stream to populate an internal list of future calendar events.
 *
 * @param url The null-terminated string of the ICS file URL (must be HTTPS).
 * @return
 *     - ESP_OK if the fetch and parse process was initiated successfully.
 *     - ESP_FAIL or other error codes on failure.
 */
esp_err_t ics_parser_fetch_and_parse(const char *url);

/**
 * @brief Gets the list of parsed future events.
 *
 * This function should be called after `ics_parser_fetch_and_parse` completes.
 * It provides a pointer to the internal, sorted array of future events.
 *
 * @param[out] events Pointer to a pointer that will be set to the start of the event array.
 * @return The number of events in the array.
 */
int ics_parser_get_events(calendar_event_t **events);

#endif // ICS_PARSER_H
