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
#include "weather.h"
#include "thingspeak.h"

static const char* TAG = "Altimeter";

/* Define pins to connect I2C pressure sensor
*/
#define I2C_PIN_SDA 25  // 25 - DevKitJ and Altimeter, 26 - Core Board
#define I2C_PIN_SCL 27

// reference pressure retrieval
#define WEATHER_DATA_RETREIVAL_PERIOD 60000
RTC_DATA_ATTR static unsigned long reference_pressure = 0l;

// Discriminate altitude changes
// to calculate cumulative altitude climbed
#define ALTITUDE_DISRIMINATION 1.5

// Altitude measurement data to retain during deep sleep
RTC_DATA_ATTR static float altitude_climbed = 0.0;
RTC_DATA_ATTR static float altitude_last;  // last measurement for cumulative calculations

// Deep sleep period in seconds
#define DEEP_SLEEP_PERIOD 15
RTC_DATA_ATTR static unsigned long boot_count = 0l;

static int blink_delay = 1000;

void intit_blink_leds()
{
    gpio_pad_select_gpio(RED_BLINK_GPIO);
    gpio_set_direction(RED_BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio(GREEN_BLINK_GPIO);
    gpio_set_direction(GREEN_BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio(BLUE_BLINK_GPIO);
    gpio_set_direction(BLUE_BLINK_GPIO, GPIO_MODE_OUTPUT);
}

void weather_data_retreived(uint32_t *args)
{
    weather_data* weather = (weather_data*) args;

    reference_pressure = (unsigned long) (weather->pressure * 100);
    ESP_LOGI(TAG, "Reference pressure: %lu Pa", reference_pressure);
}


/*
   Blink LED over the period
   when altimeter is active
 */
void blink_task(void *pvParameter)
{
    while(1) {
        /* Blink off (output low) */
        gpio_set_level(GREEN_BLINK_GPIO, 0);
        vTaskDelay(blink_delay / portTICK_RATE_MS);
        /* Blink on (output high) */
        gpio_set_level(GREEN_BLINK_GPIO, 1);
        vTaskDelay(20 / portTICK_RATE_MS);
    }
}

void measure_altitude()
{
    altitude_data altitude_record = {0};

    ESP_LOGI(TAG, "Now measuring altitude");
    altitude_record.pressure = (unsigned long) bmp180_read_pressure();
    altitude_record.temperature = bmp180_read_temperature();
    /* Compensate altitude measurement
       using current reference pressure, preferably at the sea level,
       obtained from weather station on internet
       Assume normal air pressure at sea level of 101325 Pa
       in case weather station is not available.
     */
    altitude_record.reference_pressure = reference_pressure;
    altitude_record.altitude = bmp180_read_altitude(reference_pressure);
    ESP_LOGI(TAG, "Altitude %0.1f m", altitude_record.altitude);

    float altitude_delta = altitude_record.altitude - altitude_last;
    if (altitude_delta > ALTITUDE_DISRIMINATION) {
        altitude_climbed += altitude_delta;
        ESP_LOGD(TAG, "Altitude climbed  %0.1f m", altitude_climbed);
    }
    altitude_last = altitude_record.altitude;
    altitude_record.altitude_climbed = altitude_climbed;

    time_t now = 0;
    if (time(&now) == -1) {
        ESP_LOGW(TAG, "Current calendar time is not available");
    } else {
        altitude_record.timestamp = now;
    }
    // ToDo: Calculate module up time
    // in case time is synchronized with NTP server
    // altitude_record.up_time = esp_log_timestamp()/1000l;
    altitude_record.up_time = (unsigned long) now;

    if (network_is_alive() == true) {
            thinkgspeak_post_data(&altitude_record);
    } else {
        ESP_LOGW(TAG, "Wi-Fi connection is missing");
    }
}

void app_main()
{
    ESP_LOGI(TAG, "Starting");

    boot_count++;
    ESP_LOGI(TAG, "Boot count: %lu", boot_count);

    intit_blink_leds();

    xTaskCreate(&blink_task, "blink_task", 512, NULL, 5, NULL);
    ESP_LOGI(TAG, "Blink task started");

    nvs_flash_init();
    initialise_wifi();

    blink_delay= 500;

    if (reference_pressure == 0l || boot_count % 3 == 0) {
        initialise_weather_data_retrieval(WEATHER_DATA_RETREIVAL_PERIOD);
        on_weather_data_retrieval(weather_data_retreived);
        ESP_LOGW(TAG, "Weather data retrieval initialized");
    }

    thinkgspeak_initialise();
    ESP_LOGI(TAG, "Posting to ThingSpeak initialized");

    int count_down = 5;
    // first time update of reference pressure
    if (reference_pressure == 0l) {
        ESP_LOGI(TAG, "Waiting for reference pressure update...");
        while (1) {
            ESP_LOGI(TAG, "Waiting %d", count_down);
            vTaskDelay(1000 / portTICK_RATE_MS);
            if (reference_pressure != 0) {
                ESP_LOGI(TAG, "Update received");
                break;
            }
            if (--count_down == 0) {
                reference_pressure = 101325l;
                ESP_LOGW(TAG, "Exit waiting. Assumed standard pressure at the sea level");
                break;
            }
        }
    }

    esp_err_t err = bmp180_init(I2C_PIN_SDA, I2C_PIN_SCL);
    if(err == ESP_OK){
        measure_altitude();
    } else {
        ESP_LOGE(TAG, "BMP180 init failed with error = %d", err);
        gpio_set_level(RED_BLINK_GPIO, 1);
        vTaskDelay(3000);
    }

    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", DEEP_SLEEP_PERIOD);
    esp_deep_sleep(1000000LL * DEEP_SLEEP_PERIOD);
}
