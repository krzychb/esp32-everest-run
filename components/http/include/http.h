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

extern EventGroupHandle_t wifi_event_group;
extern const int CONNECTED_BIT;

typedef void (*http_callback)(uint32_t *args);

typedef struct  {
	char *chunk_buffer;
	http_callback http_connected_cb;
	http_callback http_process_chunk_cb;
	http_callback http_disconnected_cb;
} http_client_data;

void http_client_on_connected(http_client_data *client, http_callback http_connected_cb);
void http_client_on_process_chunk(http_client_data *client, http_callback http_process_chunk_cb);
void http_client_on_disconnected(http_client_data *client, http_callback http_disconnected_cb);
void http_client_request(http_client_data *client, const char *web_server, const char *request_string);


#ifdef __cplusplus
}
#endif

#endif
