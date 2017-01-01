/* 
 sntp.h - NTP synchronisation routines

 This file is part of the ESP32 Everest Run project
 https://github.com/krzychb/esp32-everest-run

 Copyright (c) 2016 Krzysztof Budzynski <krzychb@gazeta.pl>
 This work is licensed under the Apache License, Version 2.0, January 2004
 See the file LICENSE for details.
*/
#ifndef SNTP_H
#define SNTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/event_groups.h"

extern EventGroupHandle_t wifi_event_group;
extern const int CONNECTED_BIT;

void sync_time(void);

#ifdef __cplusplus
}
#endif

#endif
