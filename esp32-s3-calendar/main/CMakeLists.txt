idf_component_register(SRCS "esp32-s3-calendar.c"
                    "wifi/wifi_manager.c" "wifi/wifi_storage.c" "wifi/softap_provisioning.c"
                    "ics/ics_parser.c"
                    "time/sntp_time.c" "time/ds3231.c"
                    "epaper/device.c" "epaper/epd_7in5_v2.c"
                    INCLUDE_DIRS "." "wifi" "ics" "time" "epaper")
