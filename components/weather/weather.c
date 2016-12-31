/*
 weather.c - Weather data retrieval from api.openweathermap.org

 This file is part of the ESP32 Everest Run project
 https://github.com/krzychb/esp32-everest-run

 Copyright (c) 2016 Krzysztof Budzynski <krzychb@gazeta.pl>
 This work is licensed under the Apache License, Version 2.0, January 2004
 See the file LICENSE for details.
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include <string.h>

#include "include/weather.h"
#include "http.h"

static const char* TAG = "Weather";

// data retrieval period in miliesconds
static unsigned long data_retrieval_period = 60000;

// Constants that aren't configurable in menuconfig
#define WEB_SERVER "api.openweathermap.org"
#define WEB_URL "http://api.openweathermap.org/data/2.5/weather?id=756135&appid"

// The API key below is configurable in menuconfig
#define OPENWEATHERMAP_API_KEY CONFIG_OPENWEATHERMAP_API_KEY

static const char *get_request = "GET " WEB_URL"="OPENWEATHERMAP_API_KEY" HTTP/1.1\n"
    "Host: "WEB_SERVER"\n"
	"Connection: close\n"
    "User-Agent: esp-idf/1.0 esp32\n"
    "\n";

static http_client_data http_client = {0};

void process_chunk(uint32_t *args)
{
    http_client_data* client = (http_client_data*)args;

    /* Collect chunks of data received from server
       into complete message saved in proc_buf
     */
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

void disconnected(uint32_t *args)
{
    http_client_data* client = (http_client_data*)args;

    printf("%s\n", client->proc_buf);
    free(client->proc_buf);
    client->proc_buf = NULL;
    client->proc_buf_size = 0;
    printf("Free heap %u\n", xPortGetFreeHeapSize());
}

void http_request_task(void *pvParameter)
{
    while(1) {
    	http_client_request(&http_client, WEB_SERVER, get_request);
        vTaskDelay(data_retrieval_period / portTICK_RATE_MS);
    }
}

void on_weather_data_retrieval(weather_data *weather, weather_data_callback data_retreived_cb)
{
	weather->data_retreived_cb = data_retreived_cb;
}

void initialise_weather_data_retrieval(weather_data *weather, unsigned long retreival_period)
{
	data_retrieval_period = retreival_period;

	http_client_on_process_chunk(&http_client, process_chunk);
    http_client_on_disconnected(&http_client, disconnected);

    xTaskCreate(&http_request_task, "http_request_task", 2 * 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "HTTP request task started");
}
