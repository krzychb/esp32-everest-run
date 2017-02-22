/*
 weather_pw.c - Weather data retrieval from MeteoStation
                at Faculty of Physics, Warsaw University of Technology

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

#include <stdio.h>
#include <string.h>

#include "weather_pw.h"
#include "http.h"
#include "jsmn.h"

static const char* TAG = "Weather PW";

/* Constants that aren't configurable in menuconfig
   Typically only LOCATION_ID may need to be changed
 */
#define WEB_SERVER "www.if.pw.edu.pl"
#define WEB_URL "http://www.if.pw.edu.pl/~meteo/okienkow.php"

// The API key below is configurable in menuconfig
#define OPENWEATHERMAP_API_KEY CONFIG_OPENWEATHERMAP_API_KEY

static const char *get_request = "GET "WEB_URL" HTTP/1.1\n"
    "Host: "WEB_SERVER"\n"
    "Connection: close\n"
    "Cache-Control: no-cache\n"
    "User-Agent: esp-idf esp32\n"
    "\n";

static weather_pw_data weather;
static http_client_data http_client = {0};

/* Collect chunks of data received from server
   into complete message and save it in proc_buf
 */
static void process_chunk(uint32_t *args)
{
    http_client_data* client = (http_client_data*)args;

    int proc_buf_new_size = client->proc_buf_size + client->recv_buf_size;
    char *copy_from;

    if (client->proc_buf == NULL){
        client->proc_buf = malloc(proc_buf_new_size);
        copy_from = client->proc_buf;
    } else {
        proc_buf_new_size -= 1; // chunks of data are '\0' terminated
        client->proc_buf = realloc(client->proc_buf, proc_buf_new_size);
        copy_from = client->proc_buf + proc_buf_new_size - client->recv_buf_size;
    }
    if (client->proc_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory");
    }
    client->proc_buf_size = proc_buf_new_size;
    memcpy(copy_from, client->recv_buf, client->recv_buf_size);
}


static bool process_response_body(const char * response_body)
{
    /* Phrase web page and extract pressure value
       Return true if phrasing was successful or false if otherwise
     */

    char* str_pos = strstr(response_body, "hPa");
    if (str_pos != NULL) {
        *str_pos = '\0';  // terminate string where found 'hPa'
        // identify beginning of pressure value in the string
        char* str_pressure = str_pos - 7;  // actual value is > 999.9 hPa
        if (str_pressure[0] < '0' || str_pressure[0] > '9') {
            str_pressure = str_pos - 6;  // actual value is < 1000.0 hPa
        }
        ESP_LOGI(TAG, "Atmospheric pressure (str): %s", str_pressure);
        // search for ',' and replace it with '.' as it is used alternatively as a decimal point
        str_pos = strchr(str_pressure, ',');
        if (str_pos != NULL) {
            *str_pos = '.';
        }
        weather.pressure = atof(str_pressure);  // convert string to float
        ESP_LOGI(TAG, "Atmospheric pressure (float): %0.1f", weather.pressure);
        return true;
    } else {
        ESP_LOGE(TAG, "Could not find any atmospheric pressure value");
    }
    return false;
}


static void disconnected(uint32_t *args)
{
    http_client_data* client = (http_client_data*)args;
    bool weather_pw_data_phrased = false;

    const char * response_body = find_response_body(client->proc_buf);
    //
    // ToDo: Remove diagnostics
    //
    // printf("%s", response_body);

    if (response_body) {
        weather_pw_data_phrased = process_response_body(response_body);
    } else {
        ESP_LOGE(TAG, "No HTTP header found");
    }

    free(client->proc_buf);
    client->proc_buf = NULL;
    client->proc_buf_size = 0;

    // execute callback if data was retrieved
    if (weather_pw_data_phrased) {
        if (weather.data_retreived_cb) {
            weather.data_retreived_cb((uint32_t*) &weather);
        }
    }
    ESP_LOGD(TAG, "Free heap %u", xPortGetFreeHeapSize());
}

static void http_request_task(void *pvParameter)
{
    while(1) {
        http_client_request(&http_client, WEB_SERVER, get_request);
        vTaskDelay(weather.retreival_period / portTICK_RATE_MS);
    }
}

void on_weather_pw_data_retrieval(weather_pw_data_callback data_retreived_cb)
{
    weather.data_retreived_cb = data_retreived_cb;
}

void initialise_weather_pw_data_retrieval(unsigned long retreival_period)
{
    weather.retreival_period = retreival_period;

    http_client_on_process_chunk(&http_client, process_chunk);
    http_client_on_disconnected(&http_client, disconnected);

    xTaskCreate(&http_request_task, "http_request_task", 3 * 1024, NULL, 5, NULL);
    ESP_LOGI(TAG, "HTTP request task started");
}

void update_weather_pw_data_retrieval(unsigned long retreival_period)
{
    weather.retreival_period = retreival_period;
}
