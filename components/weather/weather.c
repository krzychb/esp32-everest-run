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

#include "weather.h"
#include "http.h"
#include "jsmn.h"

static const char* TAG = "Weather";

/* Constants that aren't configurable in menuconfig
   Typically only LOCATION_ID may need to be changed
 */
#define WEB_SERVER "api.openweathermap.org"
#define WEB_URL "http://api.openweathermap.org/data/2.5/weather"
// Location ID to get the weather data for
#define LOCATION_ID "756135"

// The API key below is configurable in menuconfig
#define OPENWEATHERMAP_API_KEY CONFIG_OPENWEATHERMAP_API_KEY

static const char *get_request = "GET " WEB_URL"?id="LOCATION_ID"&appid="OPENWEATHERMAP_API_KEY" HTTP/1.1\n"
    "Host: "WEB_SERVER"\n"
    "Connection: close\n"
    "User-Agent: esp-idf/1.0 esp32\n"
    "\n";

static weather_data* s_weather;
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


/* Out of HTTP response return pointer to response body
   Function return NULL if end of header cannot be identified
 */
static const char * find_response_body(char * response)
{
    // Identify end of the response headers
    // http://stackoverflow.com/questions/11254037/how-to-know-when-the-http-headers-part-is-ended
    char * eol; // end of line
    char * bol; // beginning of line
    bool nheaderfound = false; // end of response headers has been found

    bol = response;
    while ((eol = index(bol, '\n')) != NULL) {
        // update bol based upon the value of eol
        bol = eol + 1;
        // test if end of headers has been reached
        if ( (!(strncmp(bol, "\r\n", 2))) || (!(strncmp(bol, "\n", 1))) )
        {
           // note that end of headers has been found
            nheaderfound = true;
           // update the value of bol to reflect the beginning of the line
           // immediately after the headers
           if (bol[0] != '\n') {
              bol += 1;
           }
           bol += 1;
           break;
        }
    }
    if (nheaderfound) {
        return bol;
    } else {
        return NULL;
    }
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
    if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
            strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return 0;
    }
    return -1;
}

static bool process_response_body(const char * body)
{
    /* Using great little JSON parser http://zserge.com/jsmn.html
       find specific weather information:
         - Humidity,
         - Temperature,
         - Pressure
       in HTTP response body that happens to be a JSON string

       Return true if phrasing was successful or false if otherwise
     */

    #define JSON_MAX_TOKENS 100
    jsmn_parser parser;
    jsmntok_t t[JSON_MAX_TOKENS];
    jsmn_init(&parser);
    int r = jsmn_parse(&parser, body, strlen(body), t, JSON_MAX_TOKENS);
    if (r < 0) {
        ESP_LOGE(TAG, "JSON parse error %d", r);
        return false;
    }
    if (r < 1 || t[0].type != JSMN_OBJECT) {
        ESP_LOGE(TAG, "JSMN_OBJECT expected");
        return false;
    } else {
        ESP_LOGI(TAG, "Token(s) found %d", r);
        char subbuff[8];
        int str_length;
        for (int i = 1; i < r; i++) {
            if (jsoneq(body, &t[i], "humidity") == 0) {
                str_length = t[i+1].end - t[i+1].start;
                memcpy(subbuff, body + t[i+1].start, str_length);
                subbuff[str_length] = '\0';
                s_weather->humidity = atoi(subbuff);
                i++;
            } else if (jsoneq(body, &t[i], "temp") == 0) {
                str_length = t[i+1].end - t[i+1].start;
                memcpy(subbuff, body + t[i+1].start, str_length);
                subbuff[str_length] = '\0';
                s_weather->temperature = atof(subbuff);
                i++;
            } else if (jsoneq(body, &t[i], "pressure") == 0) {
                str_length = t[i+1].end - t[i+1].start;
                memcpy(subbuff, body + t[i+1].start, str_length);
                subbuff[str_length] = '\0';
                s_weather->pressure = atoi(subbuff);
                i++;
            }
        }
        return true;
    }
}


static void disconnected(uint32_t *args)
{
    http_client_data* client = (http_client_data*)args;
    bool weather_data_phrased = false;

    const char * response_body = find_response_body(client->proc_buf);
    if (response_body) {
        weather_data_phrased = process_response_body(response_body);
    } else {
        ESP_LOGE(TAG, "No HTTP header found");
    }

    free(client->proc_buf);
    client->proc_buf = NULL;
    client->proc_buf_size = 0;

    // execute callback if data was retrieved
    if (weather_data_phrased) {
        if (s_weather->data_retreived_cb) {
            s_weather->data_retreived_cb((uint32_t*) s_weather);
        }
    }
    ESP_LOGD(TAG, "Free heap %u", xPortGetFreeHeapSize());
}

static void http_request_task(void *pvParameter)
{
    while(1) {
        http_client_request(&http_client, WEB_SERVER, get_request);
        vTaskDelay(s_weather->retreival_period / portTICK_RATE_MS);
    }
}

void on_weather_data_retrieval(weather_data *weather, weather_data_callback data_retreived_cb)
{
    weather->data_retreived_cb = data_retreived_cb;
}

void initialise_weather_data_retrieval(weather_data *weather, unsigned long retreival_period)
{
    s_weather = weather;
    weather->retreival_period = retreival_period;

    http_client_on_process_chunk(&http_client, process_chunk);
    http_client_on_disconnected(&http_client, disconnected);

    xTaskCreate(&http_request_task, "http_request_task", 2 * 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "HTTP request task started");
}
