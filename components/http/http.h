/* 
 http.h - HTTP request routines

 This file is part of the ESP32 Everest Run project
 https://github.com/krzychb/esp32-everest-run

 Copyright (c) 2016 Krzysztof Budzynski <krzychb@gazeta.pl>
 This work is licensed under the Apache License, Version 2.0, January 2004
 See the file LICENSE for details.
*/
#ifndef HTTP_H
#define HTTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/event_groups.h"
#include "esp_err.h"

extern EventGroupHandle_t wifi_event_group;
extern const int CONNECTED_BIT;

typedef void (*http_callback)(uint32_t *args);

typedef struct {
    char *recv_buf;
    int recv_buf_size;
    char *proc_buf;
    int proc_buf_size;
    http_callback http_connected_cb;
    http_callback http_process_chunk_cb;
    http_callback http_disconnected_cb;
} http_client_data;

#define ESP_ERR_HTTP_BASE 0x40000
#define ESP_ERR_HTTP_DNS_LOOKUP_FAILED          (ESP_ERR_HTTP_BASE + 1)
#define ESP_ERR_HTTP_FAILED_TO_ALLOCATE_SOCKET  (ESP_ERR_HTTP_BASE + 2)
#define ESP_ERR_HTTP_SOCKET_CONNECT_FAILED      (ESP_ERR_HTTP_BASE + 3)
#define ESP_ERR_HTTP_SOCKET_SEND_FAILED         (ESP_ERR_HTTP_BASE + 4)

void http_client_on_connected(http_client_data *client, http_callback http_connected_cb);
void http_client_on_process_chunk(http_client_data *client, http_callback http_process_chunk_cb);
void http_client_on_disconnected(http_client_data *client, http_callback http_disconnected_cb);
esp_err_t http_client_request(http_client_data *client, const char *web_server, const char *request_string);


#ifdef __cplusplus
}
#endif

#endif
