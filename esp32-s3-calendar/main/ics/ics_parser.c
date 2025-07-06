#include "ics_parser.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"

// --- Constants from Demo ---
#define MAX_HTTP_RECV_BUFFER    1024
#define MAX_ICS_LINE_LEN        256
#define MAX_DT_STR_LEN          32

static const char *TAG = "ICS_PARSER";

// --- Global Event Array and Count ---
static calendar_event_t future_events[MAX_EVENTS];
static int future_event_count = 0;

// --- Forward Declarations ---
static esp_err_t _http_event_handler(esp_http_client_event_t *evt);
static void parse_ics_data(const char *ics_data_chunk, size_t len);
static int manual_parse_dtstart(const char* dt_str_in, struct tm *t_out, bool *is_utc_out);
static int compare_events(const void *a, const void *b);

// --- HTTP Event Handler ---
esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (evt->user_data) {
                 parse_ics_data(evt->data, evt->data_len);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
         case HTTP_EVENT_REDIRECT:
             ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
             esp_http_client_set_header(evt->client, "From", "user@example.com");
             esp_http_client_set_header(evt->client, "Accept", "text/html");
             esp_http_client_set_redirection(evt->client);
             break;
    }
    return ESP_OK;
}

// --- Public Function: Fetch and Parse ---
esp_err_t ics_parser_fetch_and_parse(const char *url) {
    future_event_count = 0;
    memset(future_events, 0, sizeof(future_events));

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .user_data = future_events, // Context for the handler
        .disable_auto_redirect = false,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .skip_cert_common_name_check = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
         ESP_LOGE(TAG, "Failed to initialize HTTP client");
         return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS GET Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        // After fetching, sort the events
        if (future_event_count > 0) {
            qsort(future_events, future_event_count, sizeof(calendar_event_t), compare_events);
            ESP_LOGI(TAG, "%d future events sorted.", future_event_count);
        }
    } else {
        ESP_LOGE(TAG, "HTTPS GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

// --- Public Function: Get Events ---
int ics_parser_get_events(calendar_event_t **events) {
    if (events) {
        *events = future_events;
    }
    return future_event_count;
}

// --- Comparison function for qsort ---
static int compare_events(const void *a, const void *b) {
    calendar_event_t *eventA = (calendar_event_t *)a;
    calendar_event_t *eventB = (calendar_event_t *)b;
    if (eventA->start_time < eventB->start_time) return -1;
    if (eventA->start_time > eventB->start_time) return 1;
    return 0;
}

// --- Internal Parsing Logic (from demo) ---
static int manual_parse_dtstart(const char* dt_str_in, struct tm *t_out, bool *is_utc_out) {
    memset(t_out, 0, sizeof(struct tm)); 
    t_out->tm_isdst = -1; // Let mktime determine DST
    if (is_utc_out) *is_utc_out = false;

    char cleaned_dt_str[MAX_DT_STR_LEN]; 
    const char *p_in = dt_str_in;
    int i = 0;

    while (*p_in && isspace((unsigned char)*p_in)) p_in++;
    while (*p_in && i < MAX_DT_STR_LEN - 1) cleaned_dt_str[i++] = *p_in++;
    cleaned_dt_str[i] = '\0';
    i--;
    while (i >= 0 && isspace((unsigned char)cleaned_dt_str[i])) cleaned_dt_str[i--] = '\0';
    
    int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;
    int consumed_chars = 0;
    int n_parsed_items = sscanf(cleaned_dt_str, "%4d%2d%2dT%2d%2d%2d%n", &year, &month, &day, &hour, &min, &sec, &consumed_chars);

    if (n_parsed_items == 6) { 
        if (is_utc_out && cleaned_dt_str[consumed_chars] == 'Z') *is_utc_out = true;
    } else {
        hour = 0; min = 0; sec = 0; consumed_chars = 0;
        n_parsed_items = sscanf(cleaned_dt_str, "%4d%2d%2d%n", &year, &month, &day, &consumed_chars);
        if (n_parsed_items != 3) {
            ESP_LOGW(TAG, "Manual sscanf parse failed for DTSTART: [%s]", cleaned_dt_str);
            return 0; 
        }
    }

    if (year < 1970 || year > 2038 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 60) {
        ESP_LOGW(TAG, "Parsed date/time components out of valid range");
    }

    t_out->tm_year = year - 1900;
    t_out->tm_mon = month - 1; 
    t_out->tm_mday = day;
    t_out->tm_hour = hour;
    t_out->tm_min = min;
    t_out->tm_sec = sec;

    return 1; 
}

static char line_buffer[MAX_ICS_LINE_LEN] = {0};
static int line_buffer_len = 0;
static bool in_vevent = false;
static calendar_event_t current_event = {0};

static void process_ics_line(const char *line) {
    if (strncmp(line, "BEGIN:VEVENT", 12) == 0) {
        in_vevent = true;
        memset(&current_event, 0, sizeof(current_event)); 
        return;
    }
    if (strncmp(line, "END:VEVENT", 10) == 0) {
        if (in_vevent) {
            time_t now_utc; time(&now_utc);
            if (current_event.start_time > 0 && current_event.start_time > now_utc) {
                 if (future_event_count < MAX_EVENTS) {
                    future_events[future_event_count++] = current_event;
                 } else {
                      ESP_LOGW(TAG, "Max future events limit reached (%d)", MAX_EVENTS);
                 }
            }
        }
        in_vevent = false;
        return;
    }

    if (in_vevent) {
        if (strncmp(line, "SUMMARY", 7) == 0) {
            const char *summary_start = strchr(line, ':');
            if (summary_start) {
                summary_start++;
                while(*summary_start && isspace((unsigned char)*summary_start)) summary_start++;
                strncpy(current_event.summary, summary_start, MAX_SUMMARY_LEN - 1);
                current_event.summary[MAX_SUMMARY_LEN - 1] = '\0'; 
                char *summary_end = current_event.summary + strlen(current_event.summary) - 1;
                while(summary_end >= current_event.summary && isspace((unsigned char)*summary_end)) *summary_end-- = '\0';
            }
        } else if (strncmp(line, "DTSTART", 7) == 0) {
            const char *dt_value_raw = strchr(line, ':');
            if (dt_value_raw) {
                dt_value_raw++; 
                struct tm parsed_tm;
                bool is_event_utc = false;
                if (manual_parse_dtstart(dt_value_raw, &parsed_tm, &is_event_utc)) {
                    time_t event_time_t;

                    // Temporarily set TZ for mktime based on event type
                    if (is_event_utc) {
                        setenv("TZ", "UTC0", 1);
                        tzset();
                        event_time_t = mktime(&parsed_tm);
                    } else {
                        setenv("TZ", "CST-8", 1);
                        tzset();
                        event_time_t = mktime(&parsed_tm);
                    }

                    // If the original event time was NOT UTC, it means it was local (CST-8).
                    // mktime with TZ=UTC0 would have treated parsed_tm as UTC.
                    // So, we need to adjust the UTC event_time_t by subtracting the CST-8 offset (8 hours)
                    // to get the true UTC timestamp corresponding to that local time.
                    if (!is_event_utc) {
                        event_time_t -= 8 * 3600; // Subtract 8 hours to get UTC from CST-8
                    }

                    // Always restore TZ to CST-8 after mktime call
                    setenv("TZ", "CST-8", 1);
                    tzset();

                    current_event.start_time = (event_time_t == (time_t)-1) ? (time_t)-1 : event_time_t;
                } else {
                    current_event.start_time = (time_t)-1; 
                }
            }
        }
    }
}

static void parse_ics_data(const char *ics_data_chunk, size_t len) {
    const char *p = ics_data_chunk;
    const char *end = ics_data_chunk + len;
    while (p < end) {
        char c = *p++;
        if (c == '\n') {
            if (line_buffer_len > 0 && line_buffer[line_buffer_len - 1] == '\r') {
                line_buffer[line_buffer_len - 1] = '\0';
            } else {
                line_buffer[line_buffer_len] = '\0';
            }
            if (line_buffer_len > 0) process_ics_line(line_buffer);
            line_buffer_len = 0;
        } else {
            if (line_buffer_len < MAX_ICS_LINE_LEN - 1) {
                line_buffer[line_buffer_len++] = c;
            } else {
                 line_buffer_len = 0;
                 while(p < end && *p != '\n') p++;
                 if (p < end) p++;
            }
        }
    }
}
