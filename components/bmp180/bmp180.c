/*
 bmp180.c - BMP180 pressure sensor driver for ESP32

 This file is part of the ESP32 Everest Run project
 https://github.com/krzychb/esp32-everest-run

 Copyright (c) 2016 Krzysztof Budzynski <krzychb@gazeta.pl>
 This work is licensed under the Apache License, Version 2.0, January 2004
 See the file LICENSE for details.
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"

#include <math.h>

#include "bmp180.h"
#include "twi.h"

static const char* TAG = "BMP180";

#define BMP180_ADDRESS 0x77  // I2C address of BMP180

#define BMP180_ULTRA_LOW_POWER  0
#define BMP180_STANDARD         1
#define BMP180_HIGH_RES         2
#define BMP180_ULTRA_HIGH_RES   3

#define BMP180_CAL_AC1          0xAA  // Calibration data (16 bits)
#define BMP180_CAL_AC2          0xAC  // Calibration data (16 bits)
#define BMP180_CAL_AC3          0xAE  // Calibration data (16 bits)
#define BMP180_CAL_AC4          0xB0  // Calibration data (16 bits)
#define BMP180_CAL_AC5          0xB2  // Calibration data (16 bits)
#define BMP180_CAL_AC6          0xB4  // Calibration data (16 bits)
#define BMP180_CAL_B1           0xB6  // Calibration data (16 bits)
#define BMP180_CAL_B2           0xB8  // Calibration data (16 bits)
#define BMP180_CAL_MB           0xBA  // Calibration data (16 bits)
#define BMP180_CAL_MC           0xBC  // Calibration data (16 bits)
#define BMP180_CAL_MD           0xBE  // Calibration data (16 bits)

#define BMP180_CONTROL             0xF4  // Control register
#define BMP180_DATA_TO_READ        0xF6  // Read results here
#define BMP180_READ_TEMP_CMD       0x2E  // Request temperature measurement
#define BMP180_READ_PRESSURE_CMD   0x34  // Request pressure measurement

// Calibration parameters
static int16_t ac1;
static int16_t ac2;
static int16_t ac3;
static uint16_t ac4;
static uint16_t ac5;
static uint16_t ac6;
static int16_t b1;
static int16_t b2;
static int16_t mb;
static int16_t mc;
static int16_t md;
static uint8_t oversampling = BMP180_ULTRA_HIGH_RES;

int bmp180_read_int16(uint8_t reg)
{
    uint8_t buff[2] = {0};
    int16_t data = 0;

    int rc = twi_writeTo(BMP180_ADDRESS, &reg, 1, true);
    if (rc == 0) {
        rc = twi_readFrom(BMP180_ADDRESS, buff, 2, true);
        if (rc == 0) {
            data = (int16_t) buff[0]<<8 | buff[1];
        }
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "Read [%02x] failed rc=%d", reg, rc);
    }
    return data;
}

uint8_t bmp180_write(uint8_t reg, uint8_t data)
{
    uint8_t ret=0;
    uint8_t buf[] = {reg, data};

    ret = twi_writeTo(BMP180_ADDRESS, buf, 2, true);
    if (ret != 0) {
        ESP_LOGE(TAG, "Write [%02x]=%02x failed", reg, data);
    }
    return ret;
}


int16_t bmp180_read_uncompensated_temperature()
{
    bmp180_write(BMP180_CONTROL, BMP180_READ_TEMP_CMD);
    vTaskDelay(5 / portTICK_RATE_MS);
    return bmp180_read_int16(BMP180_DATA_TO_READ);
}

int16_t calculate_b5()
{
    int16_t ut;
    int32_t x1, x2;

    ut = bmp180_read_uncompensated_temperature();
    x1 = ((ut - (int32_t) ac6) * (int32_t) ac5) >> 15;
    x2 = ((int32_t) mc << 11) / (x1 + md);
    return x1 + x2;
}

float bmp180_read_temperature(void)
{
    int16_t b5;

    b5 = calculate_b5();
    return ((b5 + 8) >> 4) / 10.0;
}


uint32_t bmp180_read_uncompensated_pressure(void)
{
    bmp180_write(BMP180_CONTROL, BMP180_READ_PRESSURE_CMD + (oversampling << 6));

    uint8_t wait_time_ms = 2 + (3 << oversampling);
    vTaskDelay(wait_time_ms / portTICK_RATE_MS);

    uint8_t reg = BMP180_DATA_TO_READ;
    uint8_t buff[3] = {0};
    uint32_t data = 0;

    int rc = twi_writeTo(BMP180_ADDRESS, &reg, 1, true);
    if (rc == 0) {
        rc = twi_readFrom(BMP180_ADDRESS, buff, 3, true);
        if (rc == 0) {
            data = (uint32_t) buff[0]<<16 | buff[1]<<8 | buff[2];
            data >>= (8 - oversampling);
        }
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "Read [%02x] failed rc=%d", reg, rc);
    }
    return data;
}

uint32_t bmp180_read_pressure(void)
{
    int32_t up, b3, b5, b6, x1, x2, x3, p;
    uint32_t b4, b7;

    b5 = calculate_b5();
    b6 = b5 - 4000;

    x1 = (b2 * (b6 * b6) >> 12) >> 11;
    x2 = (ac2 * b6) >> 11;
    x3 = x1 + x2;
    b3 = (((((int32_t)ac1) * 4 + x3) << oversampling) + 2) >> 2;

    x1 = (ac3 * b6) >> 13;
    x2 = (b1 * ((b6 * b6) >> 12)) >> 16;
    x3 = ((x1 + x2) + 2) >> 2;
    b4 = (ac4 * (uint32_t)(x3 + 32768)) >> 15;

    up = bmp180_read_uncompensated_pressure();
    b7 = ((uint32_t)(up - b3) * (50000 >> oversampling));
    if (b7 < 0x80000000) {
        p = (b7 << 1) / b4;
    } else {
        p = (b7 / b4) << 1;
    }

    x1 = (p >> 8) * (p >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;
    p += (x1 + x2 + 3791) >> 4;

    return p;
}

float bmp180_read_altitude(unsigned long sea_level_pressure)
{
    uint32_t absolute_pressure = bmp180_read_pressure();
    return 44330 * (1.0 - powf(absolute_pressure / (float) sea_level_pressure, 0.190295));
}


esp_err_t bmp180_init(int pin_sda, int pin_scl)
{
    if (portTICK_RATE_MS > 1) {
        /* portTICK_RATE_MS should be 1ms,
           use 'make menuconfig' to update it
           by setting Tick rate to 1000
           under Component config > FreeRTOS > Tick rate (Hz)
         */
        ESP_LOGE(TAG, "FreeRTOS tick rate (%d) too slow", portTICK_RATE_MS);
        return ESP_ERR_BMP180_NOT_DETECTED;
    }

    twi_init(pin_sda, pin_scl);

    uint8_t reg = 0x00;
    if (twi_writeTo(BMP180_ADDRESS, &reg, 1, true) == 0) {
        ESP_LOGD(TAG, "Sensor found at 0x%02x", BMP180_ADDRESS);
        ac1 = bmp180_read_int16(BMP180_CAL_AC1);
        ac2 = bmp180_read_int16(BMP180_CAL_AC2);
        ac3 = bmp180_read_int16(BMP180_CAL_AC3);
        ac4 = (uint16_t) bmp180_read_int16(BMP180_CAL_AC4);
        ac5 = (uint16_t) bmp180_read_int16(BMP180_CAL_AC5);
        ac6 = (uint16_t) bmp180_read_int16(BMP180_CAL_AC6);
        b1 = bmp180_read_int16(BMP180_CAL_B1);
        b2 = bmp180_read_int16(BMP180_CAL_B2);
        mb = bmp180_read_int16(BMP180_CAL_MB);
        mc = bmp180_read_int16(BMP180_CAL_MC);
        md = bmp180_read_int16(BMP180_CAL_MD);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Sensor not found at 0x%02x", BMP180_ADDRESS);
        return ESP_ERR_BMP180_NOT_DETECTED;
    }
}
