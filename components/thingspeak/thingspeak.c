/*
 weather.c - Weather data retrieval from api.openweathermap.org

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

#include "thingspeak.h"
#include "http.h"

static const char* TAG = "ThingSpeak";

/* Constants that aren't configurable in menuconfig
   Typically only LOCATION_ID may need to be changed
 */
#define WEB_SERVER "api.thingspeak.com"

// The API key below is configurable in menuconfig
#define THINGSPEAK_WRITE_API_KEY CONFIG_THINGSPEAK_WRITE_API_KEY

static const char *get_request_start =
    "GET /update?key="
    THINGSPEAK_WRITE_API_KEY;

static const char *get_request_end =
    " HTTP/1.1\n"
    "Host: "WEB_SERVER"\n"
    "Connection: close\n"
    "User-Agent: esp-idf/1.0 esp32\n"
    "\n";

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

static void disconnected(uint32_t *args)
{
    http_client_data* client = (http_client_data*)args;

    printf("%s\n", client->proc_buf);

    free(client->proc_buf);
    client->proc_buf = NULL;
    client->proc_buf_size = 0;

    ESP_LOGD(TAG, "Free heap %u", xPortGetFreeHeapSize());
}

void thinkgspeak_post_data(altitude_data *altitude_record)
{
    int n;
    n = snprintf(NULL, 0, "%lu", altitude_record->pressure);
    char field1[n+1];
    sprintf(field1, "%lu", altitude_record->pressure);

    n = snprintf(NULL, 0, "%.1f", altitude_record->altitude);
    char field3[n+1];
    sprintf(field3, "%.1f", altitude_record->altitude);

    int string_size = strlen(get_request_start);
    string_size += strlen("&field1=");
    string_size += strlen(field1);
    string_size += strlen("&field3=");
    string_size += strlen(field3);
    string_size += strlen(get_request_end);

    char * get_request = malloc(string_size);
    strcpy(get_request, get_request_start);
    strcat(get_request, "&field1=");
    strcat(get_request, field1);
    strcat(get_request, "&field3=");
    strcat(get_request, field3);
    strcat(get_request, get_request_end);

    http_client_request(&http_client, WEB_SERVER, get_request);

    free(get_request);
}

void thinkgspeak_initialise()
{
    http_client_on_process_chunk(&http_client, process_chunk);
    http_client_on_disconnected(&http_client, disconnected);
}
