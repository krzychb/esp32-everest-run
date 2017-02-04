/* 
 keenio.h - Routines to post data to Keen.io

 This file is part of the ESP32 Everest Run project
 https://github.com/krzychb/esp32-everest-run

 Copyright (c) 2016 Krzysztof Budzynski <krzychb@gazeta.pl>
 This work is licensed under the Apache License, Version 2.0, January 2004
 See the file LICENSE for details.
*/
#ifndef KEENIO_H
#define KEENIO_H

#include "esp_err.h"
#include "altimeter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_ERR_KEENIO_BASE 0x60000
#define ESP_ERR_KEENIO_POST_FAILED          (ESP_ERR_KEENIO_BASE + 1)

void keenio_post_data(altitude_data *altitude_record, unsigned long record_count);
void keenio_initialise();

#ifdef __cplusplus
}
#endif

#endif  // KEENIO_H
