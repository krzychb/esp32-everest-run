// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_ERR_BMP180_BASE            0x30000
#define ESP_ERR_BMP180_NOT_DETECTED    (ESP_ERR_BMP180_BASE + 1)

esp_err_t bmp180_init(int pin_sda, int pin_scl);
float bmp180_read_temperature(void);
uint32_t bmp180_read_pressure(void);
float bmp180_read_altitude(uint32_t sea_level_pressure);

#ifdef __cplusplus
}
#endif
