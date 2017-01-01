/*
 sntp.c - NTP synchronisation routines

 File based on https://github.com/espressif/esp-idf/tree/master/examples/06_sntp

 This file is part of the ESP32 Everest Run project
 https://github.com/krzychb/esp32-everest-run

 Copyright (c) 2016 Krzysztof Budzynski <krzychb@gazeta.pl>
 This work is licensed under the Apache License, Version 2.0, January 2004
 See the file LICENSE for details.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include <time.h>
#include <sys/time.h>
#include "apps/sntp/sntp.h"

#include "sntp.h"

static const char* TAG = "SNTP";

#define NTP_MAX_RETRIES 10

void sync_time(void)
{
    ESP_LOGI(TAG, "Waiting for Wi-Fi");
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                       false, true, portMAX_DELAY);

    ESP_LOGI(TAG, "Initialising");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < NTP_MAX_RETRIES) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, NTP_MAX_RETRIES);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    char strftime_buf[64];

    // Set timezone to Central European Time and print local time
    setenv("TZ", "CET-1CEST,M3.5.0/1,M10.5.0/1", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Warsaw is: %s", strftime_buf);
}
