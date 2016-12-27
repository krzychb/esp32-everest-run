# esp32-everest-run

Internet connected altimeter for Everest Run - http://everestrun.pl/

## Components

1. [ESP32](https://espressif.com/en/products/hardware/esp32/overview) Wi-Fi + Bluetooth Combo Chip by [Espressif](https://espressif.com/)
2. [BMP180]( https://www.bosch-sensortec.com/bst/products/all_products/bmp180) Digital pressure sensor
3. [esp-idf](https://github.com/espressif/esp-idf)
 Espressif IoT Development Framework for ESP32

## Build Status

[![Build Status](https://travis-ci.org/krzychb/esp32-everest-run.svg?branch=master)](https://travis-ci.org/krzychb/esp32-everest-run)

## Work Progress

- [x] Prepare a prototype using ESP32-DevKitJ-v1 development board
- [x] Develop an I2C driver to read altitude from BMP180 sensor
- [x] Synchronize time on ESP32 by obtaining it from a NTP server
- [ ] Implement compensation of altitude measurement using absolute pressure obtained on-line from internet
- [ ] Publish altitude measurements in real time on [ThingSpeak](https://thingspeak.com/)
- [ ] Implement data buffering on ESP32 for the period of likely Wi-Fi connection losses
- [ ] Build lightweight, battery powered altimeter using bare [ESP-WROOM-32](https://espressif.com/en/products/hardware/esp-wroom-32/overview) module
- [ ] Test altimeter during weekly trainings organized by the group [Biegamy po schodach](https://www.facebook.com/groups/biegamyposchodach/)
- [ ] Complete [Everest Run](http://everestrun.pl/) by climbing 2730 floors on 18 and 19 February 2017

