/* 
 logger.k - save altimeter data on sd card

 This file is part of the ESP32 Everest Run project
 https://github.com/krzychb/esp32-everest-run

 Copyright (c) 2016 Krzysztof Budzynski <krzychb@gazeta.pl>
 This work is licensed under the Apache License, Version 2.0, January 2004
 See the file LICENSE for details.
*/
#ifndef LOGGER_H
#define LOGGER_H

#include <time.h>
#include "esp_err.h"

#include "altimeter.h"

#ifdef __cplusplus
extern "C" {
#endif


#define ESP_ERR_LOGGER_BASE 0x50000
#define ESP_ERR_LOGGER_DIR_OPEN_FAILED          (ESP_ERR_LOGGER_BASE + 1)
#define ESP_ERR_LOGGER_DIR_CLOSE_FAILED         (ESP_ERR_LOGGER_BASE + 2)
#define ESP_ERR_LOGGER_FILE_OPEN_READ_FAILED    (ESP_ERR_LOGGER_BASE + 3)
#define ESP_ERR_LOGGER_FILE_OPEN_WRITE_FAILED   (ESP_ERR_LOGGER_BASE + 4)

esp_err_t logger_open();
esp_err_t logger_save(altitude_data altitude_record);
esp_err_t logger_peek(unsigned long* file_count);
esp_err_t logger_get_list(unsigned long* file_count, unsigned long* file_list);
esp_err_t logger_read(altitude_data* altitude_record, unsigned long* file_count, unsigned long* file_list);
esp_err_t logger_delete(unsigned long* file_count, unsigned long* file_list);
void logger_close();

#ifdef __cplusplus
}
#endif

#endif  // LOGGER_H
