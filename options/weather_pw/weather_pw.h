/* 
 weather_pw.h - Weather data retrieval from MeteoStation
                at Faculty of Physics, Warsaw University of Technology

 This file is part of the ESP32 Everest Run project
 https://github.com/krzychb/esp32-everest-run

 Copyright (c) 2016 Krzysztof Budzynski <krzychb@gazeta.pl>
 This work is licensed under the Apache License, Version 2.0, January 2004
 See the file LICENSE for details.
*/
#ifndef WEATHER_PW_H
#define WEATHER_PW_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*weather_pw_data_callback)(uint32_t *args);

typedef struct {
    float pressure;
    unsigned long retreival_period;
    weather_pw_data_callback data_retreived_cb;
} weather_pw_data;

#define ESP_ERR_WEATHER_PW_BASE 0x50000
#define ESP_ERR_WEATHER_PW_RETREIVAL_FAILED          (ESP_ERR_WEATHER_PW_BASE + 1)

void on_weather_pw_data_retrieval(weather_pw_data_callback data_retreived_cb);
void initialise_weather_pw_data_retrieval(unsigned long retreival_period);
void update_weather_pw_data_retrieval(unsigned long retreival_period);

#ifdef __cplusplus
}
#endif

#endif  // WEATHER_PW_H
