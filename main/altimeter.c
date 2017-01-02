/*
 altimeter.c - Altitude recording and transmission using ESP32 and BMP180 sensor

 This file is part of the ESP32 Everest Run project
 https://github.com/krzychb/esp32-everest-run

 Copyright (c) 2016 Krzysztof Budzynski <krzychb@gazeta.pl>
 This work is licensed under the Apache License, Version 2.0, January 2004
 See the file LICENSE for details.
*/

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "wifi.h"
#include "sntp.h"
#include "bmp180.h"
#include "weather.h"
#include "thingspeak.h"

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
#define BP180_SENSOR_READ_PERIOD 20000
altitude_data altitude_record;

#define WEATHER_DATA_REREIVAL_PERIOD 20000
weather_data weather = {0};

void weather_data_retreived(uint32_t *args)
{
    weather_data* weather = (weather_data*)args;

    ESP_LOGI(TAG, "Humidity: %u %%", weather->humidity);
    ESP_LOGI(TAG, "Temperature: %.1f dec C", weather->temperature - 273.15);
    ESP_LOGI(TAG, "Pressure: %u hPa", weather->pressure);
}

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
        altitude_record.pressure = (unsigned long) bmp180_read_pressure();
        altitude_record.temperature = bmp180_read_temperature();
        ESP_LOGI(TAG, "Pressure (BMP180) %lu Pa", altitude_record.pressure);
        ESP_LOGI(TAG, "Temperature (BMP180) %0.1f deg C", altitude_record.temperature);
        /* Compensate altitude measurement
           using current pressure at the sea level
           obtained from weather station on internet
           Assume normal air pressure at sea level of 101325 Pa
           in case weather station is not available.
         */
        unsigned long sea_level_pressure = 101325;
        if (weather.pressure > 0){
            sea_level_pressure = weather.pressure * 100l;
        }
        altitude_record.sea_level_pressure = sea_level_pressure;
        altitude_record.altitude = bmp180_read_altitude(sea_level_pressure);
        ESP_LOGI(TAG, "Altitude (BMP180) %0.1f m", altitude_record.altitude);

        thinkgspeak_post_data(&altitude_record);

        vTaskDelay(BP180_SENSOR_READ_PERIOD / portTICK_RATE_MS);
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

    initialise_weather_data_retrieval(&weather, WEATHER_DATA_REREIVAL_PERIOD);
    on_weather_data_retrieval(&weather, weather_data_retreived);
    ESP_LOGI(TAG, "Weather data retreival initialised");

    thinkgspeak_initialise();
    ESP_LOGI(TAG, "Posting to ThingSpeakinitialised");

    esp_err_t err = bmp180_init(I2C_PIN_SDA, I2C_PIN_SCL);
    if(err == ESP_OK){
        xTaskCreate(&bmp180_task, "bmp180_task", 3 * 1024, NULL, 5, NULL);
        ESP_LOGI(TAG, "BMP180 read task started");
    } else {
        ESP_LOGE(TAG, "BMP180 init failed with error = %d", err);
    }
}
