/*
 Altitude recording and transmission using ESP32 and BMP180 sensor

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
#include "nvs_flash.h"
#include "esp_log.h"

#include "wifi.h"
#include "sntp.h"
#include "http.h"
#include "bmp180.h"
#include "driver/gpio.h"

static const char* TAG = "Altimeter";

/* Period in milliseconds to repeat synchronisation
   of time kept by ESP32 internally, with NTP
*/
#define NTP_SYNCHRONISATION_PERIOD 60000


/* Can run 'make menuconfig' to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

/* Define pins to connect I2C pressure sensor
*/
#define I2C_PIN_SDA 25
#define I2C_PIN_SCL 27


// Constants that aren't configurable in menuconfig
#define WEB_SERVER "api.openweathermap.org"
#define WEB_URL "http://api.openweathermap.org/data/2.5/weather?id=756135&appid"

// The API key below is configurable in menuconfig
#define OPENWEATHERMAP_API_KEY CONFIG_OPENWEATHERMAP_API_KEY

static const char *get_request = "GET " WEB_URL"="OPENWEATHERMAP_API_KEY" HTTP/1.1\n"
    "Host: "WEB_SERVER"\n"
    "User-Agent: esp-idf/1.0 esp32\n"
    "\n";

http_client_data http_client;

void blink_task(void *pvParameter)
{
    /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    gpio_pad_select_gpio(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    while(1) {
        /* Blink off (output low) */
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(500 / portTICK_RATE_MS);
        /* Blink on (output high) */
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(500 / portTICK_RATE_MS);
    }
}

void bmp180_task(void *pvParameter)
{
    while(1) {
        ESP_LOGI(TAG, "Temperature %0.1f deg C", bmp180_read_temperature());
        ESP_LOGI(TAG, "Pressure %d Pa", bmp180_read_pressure());
        ESP_LOGI(TAG, "Altitude %0.1f m", bmp180_read_altitude(101325));
        vTaskDelay(3000 / portTICK_RATE_MS);
    }
}

void connected(uint32_t *args)
{

    ESP_LOGI(TAG, "HTTP CONNECTED");
}

void process_chunk(uint32_t *args)
{
    http_client_data* client = (http_client_data*)args;
    printf("%s", client->chunk_buffer);
}

void disconnected(uint32_t *args)
{
    ESP_LOGI(TAG, "HTTP DISCONNECTED");
}

void http_request_task(void *pvParameter)
{
    while(1) {
    	http_client_request(&http_client, WEB_SERVER, get_request);
        vTaskDelay(10000 / portTICK_RATE_MS);
    }
}

void sync_time_task(void *pvParameter)
{
    while(1) {
        sync_time();
        vTaskDelay(NTP_SYNCHRONISATION_PERIOD / portTICK_RATE_MS);
    }
}

void app_main()
{
    ESP_LOGI(TAG, "Starting");
    nvs_flash_init();
    initialise_wifi();

    xTaskCreate(&sync_time_task, "sync_time_task", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Sync time task started");

    xTaskCreate(&blink_task, "blink_task", 512, NULL, 5, NULL);
    ESP_LOGI(TAG, "Blink task started");

    esp_err_t err = bmp180_init(I2C_PIN_SDA, I2C_PIN_SCL);
    if(err == ESP_OK){
        xTaskCreate(&bmp180_task, "bmp180_task", 2048, NULL, 5, NULL);
        ESP_LOGI(TAG, "BMP180 read task started");
    } else {
        ESP_LOGE(TAG, "BMP180 init failed with error = %d", err);
    }

    http_client_on_connected(&http_client, connected);
    http_client_on_process_chunk(&http_client, process_chunk);
    http_client_on_disconnected(&http_client, disconnected);

    xTaskCreate(&http_request_task, "http_request_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "HTTP request task started");
}
