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
#include "altimeter.h"
#include "bmp180.h"
#include "wifi.h"
#include "sntp.h"
#include "weather_pw.h"
#include "thingspeak.h"
#include "keenio.h"
#include "logger.h"

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
#define I2C_PIN_SDA 26  // 25 - DevKitJ, 26 - Core Board
#define I2C_PIN_SCL 27
#define BP180_SENSOR_READ_PERIOD 15000

#define WEATHER_DATA_REREIVAL_PERIOD 60000
weather_pw_data weather = {0};

void weather_data_retreived(uint32_t *args)
{
    weather_pw_data* weather = (weather_pw_data*)args;

    ESP_LOGI(TAG, "Pressure: %0.1f hPa", weather->pressure);
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

void altitude_measure_task(void *pvParameter)
{
    altitude_data altitude_record = {0};

    while(1) {
        altitude_record.pressure = (unsigned long) bmp180_read_pressure();
        altitude_record.temperature = bmp180_read_temperature();
        /* Compensate altitude measurement
           using current reference pressure, preferably at the sea level,
           obtained from weather station on internet
           Assume normal air pressure at sea level of 101325 Pa
           in case weather station is not available.
         */
        unsigned long reference_pressure = 101325l;
        if (weather.pressure > 0) {
            reference_pressure = (unsigned long) (weather.pressure * 100);
        }
        altitude_record.reference_pressure = reference_pressure;
        altitude_record.altitude = bmp180_read_altitude(reference_pressure);
        ESP_LOGI(TAG, "Altitude %0.1f m", altitude_record.altitude);

        altitude_record.logged = false;
        altitude_record.up_time = esp_log_timestamp()/1000l;

        time_t now = 0;
        if (time(&now) == -1) {
            ESP_LOGW(TAG, "Current calendar time is not available");
        } else {
            altitude_record.timestamp = now;
        }

        if (network_is_alive() == true) {

            // post data to cloud
            thinkgspeak_post_data(&altitude_record);
            keenio_post_data(&altitude_record, 1l);

            // post archive data to cloud
            if (logger_is_open() == true) {
                unsigned long file_count;
                logger_peek(&file_count);
                if (file_count > 0) {
                    unsigned long file_list[file_count];
                    logger_get_list(&file_count, file_list);
                    altitude_data altitude_record[file_count];
                    logger_read(altitude_record, &file_count, file_list);
                    keenio_post_data(altitude_record, file_count);
                    logger_delete(&file_count, file_list);
                } else {
                    ESP_LOGI(TAG, "Logger has no any files archived");
                }
            }
        } else {
            ESP_LOGW(TAG, "Wi-Fi connection is missing");
            if (logger_is_open() == true) {
                ESP_LOGI(TAG, "Archiving the data");
                altitude_record.logged = true;
                logger_save(altitude_record);
            } else {
                ESP_LOGE(TAG, "Logger is not open, unable to archive data");
            }
        }

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

    initialise_weather_pw_data_retrieval(&weather, WEATHER_DATA_REREIVAL_PERIOD);
    on_weather_pw_data_retrieval(&weather, weather_data_retreived);
    ESP_LOGI(TAG, "Weather data retreival initialised");

    thinkgspeak_initialise();
    ESP_LOGI(TAG, "Posting to ThingSpeak initialised");

    keenio_initialise();
    ESP_LOGI(TAG, "Posting to Keen.IO initialised");


    esp_err_t err = bmp180_init(I2C_PIN_SDA, I2C_PIN_SCL);
    if(err == ESP_OK){
        xTaskCreate(&altitude_measure_task, "altitude_measure_task", 3 * 1024, NULL, 5, NULL);
        ESP_LOGI(TAG, "Altitude measure task started");
    } else {
        ESP_LOGE(TAG, "BMP180 init failed with error = %d", err);
    }

    err = logger_open();
    if(err == ESP_OK){
        ESP_LOGI(TAG, "Logger ready");
    } else {
        ESP_LOGE(TAG, "Logger initialisation failed");
    }
}
