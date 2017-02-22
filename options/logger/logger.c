/*
 logger.c - save altimeter data on sd card

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
#include "freertos/semphr.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"

#include "logger.h"

// logger is or is not initialized for data logging
static bool logger_initialized = false;

static SemaphoreHandle_t sd_card_busy;

// max 8 characters allowed for the filename
#define LOGGER_DATA_SET_MAX 99999999

// path to mount SD card
#define SD_BASE_PATH "/sdcard"

// counter of data sets saved on SD card
static unsigned long data_set_number = 0l;


esp_err_t logger_open()
{
    esp_err_t ret = ESP_OK;
    static const char* LOGGER_OPEN = "Logger open";

    ESP_LOGI(LOGGER_OPEN, "Initializing on %s", SD_BASE_PATH);

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5
    };

    sdmmc_card_t* card;
    ret = esp_vfs_fat_sdmmc_mount(SD_BASE_PATH, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(LOGGER_OPEN, "Failed to mount filesystem");
        } else {
            ESP_LOGE(LOGGER_OPEN, "Failed to initialize the card (%d)", ret);
        }
        return ret;
    }

    logger_initialized = true;
    sd_card_busy = xSemaphoreCreateBinary();
    xSemaphoreGive(sd_card_busy);

    // find the file name with biggest number
    // and assume that this is the last file
    // assign 'biggest number' + 1
    // to the filename of next file to write to
    unsigned long max_found = 0l;
    unsigned long file_count;
    logger_peek(&file_count);
    if(file_count > 0) {
        unsigned long file_list[file_count];
        logger_get_list(&file_count, file_list);
        for (unsigned long i=0; i<file_count; i++) {
            if (file_list[i] > max_found) {
                max_found = file_list[i];
            }
        }
    }

    data_set_number = max_found + 1;
    ESP_LOGI(LOGGER_OPEN, "Next data set number to save: %lu", data_set_number);

    return ret;
}


esp_err_t logger_save(altitude_data altitude_record)
{
    esp_err_t ret = ESP_OK;
    static const char* LOGGER_SAVE = "Logger save";
    char file_path[32];
    FILE* f;

    ESP_LOGI(LOGGER_SAVE, "Starting with data set %08lu on %s", data_set_number, SD_BASE_PATH);

    xSemaphoreTake(sd_card_busy, portMAX_DELAY);

    sprintf(file_path, "%s/%08lu.bin", SD_BASE_PATH, data_set_number);

    ESP_LOGI(LOGGER_SAVE, "Writting altitude data to '%s' file", file_path);
    f = fopen(file_path, "w");
    if (f == NULL) {
        ESP_LOGE(LOGGER_SAVE, "Failed to open '%s' file for writing", file_path);
        ret = ESP_ERR_LOGGER_FILE_OPEN_WRITE_FAILED;
    } else {
        fwrite(&altitude_record, sizeof(altitude_record), 1, f);
        fclose(f);
        ESP_LOGI(LOGGER_SAVE, "File written");

        data_set_number++;
        // max 8 characters allowed for the filename
        // reset data set number if too big to fit
        if (data_set_number > LOGGER_DATA_SET_MAX) {
            data_set_number = 0;
        }
    }

    xSemaphoreGive(sd_card_busy);

    return ret;
}


esp_err_t logger_peek(unsigned long* file_count)
{
    esp_err_t ret = ESP_OK;
    static const char* LOGGER_PEEK = "Logger peek";
    char file_path[32];

    ESP_LOGI(LOGGER_PEEK, "Starting on %s", SD_BASE_PATH);

    xSemaphoreTake(sd_card_busy, portMAX_DELAY);

    // Open directory
    sprintf(file_path, "%s/", SD_BASE_PATH);
    ESP_LOGI(LOGGER_PEEK, "Opening directory '%s'", file_path);
    DIR* dir = opendir(file_path);
    if (dir == NULL) {
        ESP_LOGE(LOGGER_PEEK, "Failure opening directory '%s'", file_path);
        ret = ESP_ERR_LOGGER_DIR_OPEN_FAILED;
    } else {
        // Count number of files in directory
        struct dirent* de = readdir(dir);
        *file_count = 0l;
        while (de != NULL) {
            *file_count += 1;
            de = readdir(dir);
        }
        ESP_LOGI(LOGGER_PEEK, "Found %lu file(s)", *file_count);

        // We are done, close directory
        if (closedir(dir) != 0) {
            ESP_LOGE(LOGGER_PEEK, "Failure closing directory");
            ret = ESP_ERR_LOGGER_DIR_CLOSE_FAILED;
        }
    }

    xSemaphoreGive(sd_card_busy);

    return ret;
}


esp_err_t logger_get_list(unsigned long* file_count, unsigned long* file_list)
{
    esp_err_t ret = ESP_OK;
    static const char* LOGGER_GET_LIST = "Logger get list";
    unsigned long file_count_done = 0;
    char file_path[32];

    ESP_LOGI(LOGGER_GET_LIST, "Starting on %s", SD_BASE_PATH);

    xSemaphoreTake(sd_card_busy, portMAX_DELAY);

    // Open directory
    sprintf(file_path, "%s/", SD_BASE_PATH);
    ESP_LOGI(LOGGER_GET_LIST, "Opening directory '%s'", file_path);
    DIR* dir = opendir(file_path);
    if (dir == NULL) {
        ESP_LOGE(LOGGER_GET_LIST, "Failure opening directory '%s'", file_path);
        ret = ESP_ERR_LOGGER_DIR_OPEN_FAILED;
    } else {
        // Get the list of 'file_count' number of files
        struct dirent* de = readdir(dir);
        while (de != NULL && file_count_done < *file_count) {
            char* pch = strchr(de->d_name,'.');
            if (pch != NULL) {
                *pch = '\0';
                file_list[file_count_done] = atoi(de->d_name);
                file_count_done++;
            } else {
                ESP_LOGE(LOGGER_GET_LIST, "File name format not supported");
            }
            de = readdir(dir);
        }

        // We are done, close directory
        if (closedir(dir) != 0) {
            ESP_LOGE(LOGGER_GET_LIST, "Failure closing directory");
            ret = ESP_ERR_LOGGER_DIR_CLOSE_FAILED;
        }
    }
    xSemaphoreGive(sd_card_busy);

    if (file_count_done == *file_count) {
        ESP_LOGI(LOGGER_GET_LIST, "Got %lu files", file_count_done);
    } else {
        ESP_LOGE(LOGGER_GET_LIST, "Expected to get %lu files, but found and got %lu files", *file_count, file_count_done);
        *file_count = file_count_done;
    }

    return ret;
}


esp_err_t logger_read(altitude_data* altitude_record, unsigned long* file_count, unsigned long* file_list)
{
    esp_err_t ret = ESP_OK;
    static const char* LOGGER_READ = "Logger read";
    unsigned long file_count_done = 0;
    char file_name[16];
    char file_path[32];
    FILE* f;

    ESP_LOGI(LOGGER_READ, "Starting on %s", SD_BASE_PATH);

    xSemaphoreTake(sd_card_busy, portMAX_DELAY);

    // Open directory
    sprintf(file_path, "%s/", SD_BASE_PATH);
    ESP_LOGI(LOGGER_READ, "Opening directory '%s'", file_path);
    DIR* dir = opendir(file_path);
    if (dir == NULL) {
        ESP_LOGE(LOGGER_READ, "Failure opening directory '%s'", file_path);
        ret = ESP_ERR_LOGGER_DIR_OPEN_FAILED;
    } else {
        ESP_LOGI(LOGGER_READ, "Now reading %lu file(s)", *file_count);
        struct dirent* de = readdir(dir);
        // now search through directory
        // and read contents of files from the list
        while (de != NULL && file_count_done < *file_count) {
            strcpy(file_name, de->d_name);
            char* pch = strchr(file_name,'.');
            if (pch != NULL) {
                *pch = '\0';
                unsigned long file_number = atoi(file_name);
                for (unsigned long i=0; i<*file_count; i++) {
                    if (file_list[i] == file_number) {
                        sprintf(file_path, "%s/%s", SD_BASE_PATH, de->d_name);

                        f = fopen(file_path, "r");
                        if (f == NULL) {
                            ESP_LOGE(LOGGER_READ, "Failed to open '%s' file for reading", file_path);
                            ret = ESP_ERR_LOGGER_FILE_OPEN_READ_FAILED;
                        } else {
                            if (fread(&altitude_record[file_count_done],sizeof(altitude_record[file_count_done]), 1, f) == 1){
                                file_count_done++;
                            } else {
                                ESP_LOGE(LOGGER_READ, "Error reading '%s' file", file_path);
                            }
                            fclose(f);
                        }
                        break;
                    }
                }
            } else {
                ESP_LOGE(LOGGER_READ, "File name format not supported");
            }
            de = readdir(dir);
        }

        // We are done, close directory
        if (closedir(dir) != 0) {
            ESP_LOGE(LOGGER_READ, "Failure closing directory");
            ret = ESP_ERR_LOGGER_DIR_CLOSE_FAILED;
        }
    }

    xSemaphoreGive(sd_card_busy);

    if (file_count_done == *file_count) {
        ESP_LOGI(LOGGER_READ, "Reading %lu files done", file_count_done);
    } else {
        ESP_LOGE(LOGGER_READ, "Expected to read %lu files, but found and read %lu files", *file_count, file_count_done);
        *file_count = file_count_done;
    }

    return ret;
}


esp_err_t logger_delete(unsigned long* file_count, unsigned long* file_list)
{
    esp_err_t ret = ESP_OK;
    static const char* LOGGER_DELETE = "Logger delete";
    unsigned long file_count_done = 0;
    char file_name[16];
    char file_path[32];

    ESP_LOGI(LOGGER_DELETE, "Starting on %s", SD_BASE_PATH);

    xSemaphoreTake(sd_card_busy, portMAX_DELAY);

    // Open directory
    sprintf(file_path, "%s/", SD_BASE_PATH);
    ESP_LOGI(LOGGER_DELETE, "Opening directory '%s'", file_path);
    DIR* dir = opendir(file_path);
    if (dir == NULL) {
        ESP_LOGE(LOGGER_DELETE, "Failure opening directory '%s'", file_path);
        ret = ESP_ERR_LOGGER_DIR_OPEN_FAILED;
    } else {
        ESP_LOGI(LOGGER_DELETE, "Now deleting %lu file(s)", *file_count);
        struct dirent* de = readdir(dir);
        // now search through directory
        // and delete files from the list
        while (de != NULL && file_count_done < *file_count) {
            strcpy(file_name, de->d_name);
            char* pch = strchr(file_name,'.');
            if (pch != NULL) {
                *pch = '\0';
                unsigned long file_number = atoi(file_name);
                for (unsigned long i=0; i<*file_count; i++) {
                    if (file_list[i] == file_number) {
                        sprintf(file_path, "%s/%s", SD_BASE_PATH, de->d_name);
                        if (unlink(file_path) == 0) {
                            ESP_LOGI(LOGGER_DELETE, "Deleted '%s' file", file_path);
                            file_count_done++;
                        } else {
                            ESP_LOGE(LOGGER_DELETE, "Failure deleting '%s' file", file_path);
                        }
                        break;
                    }
                }
            } else {
                ESP_LOGE(LOGGER_DELETE, "File name format not supported");
            }
            de = readdir(dir);
        }

        // We are done, close directory
        if (closedir(dir) != 0) {
            ESP_LOGE(LOGGER_DELETE, "Failure closing directory");
            ret = ESP_ERR_LOGGER_DIR_CLOSE_FAILED;
        }
    }

    xSemaphoreGive(sd_card_busy);

    if (file_count_done == *file_count) {
        ESP_LOGI(LOGGER_DELETE, "Deleted %lu files", file_count_done);
    } else {
        ESP_LOGE(LOGGER_DELETE, "Expected to delete %lu files, but found and deleted %lu files", *file_count, file_count_done);
        *file_count = file_count_done;
    }

    return ret;
}


void logger_close(void)
{
    static const char* LOGGER_CLOSE = "Logger";

    // All done, unmount partition and disable SDMMC host peripheral
    esp_vfs_fat_sdmmc_unmount();
    ESP_LOGI(LOGGER_CLOSE, "Card unmounted");
}


bool logger_is_open(void)
{
    return logger_initialized;
}
