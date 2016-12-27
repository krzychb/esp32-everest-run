#ifndef __WIRING_H__
#define __WIRING_H__

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Some functions from Arduino/Wiring
void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
void delay(int millis);

#define OUTPUT 0
#define INPUT 1
#define INPUT_PULLUP 2

#endif //__WIRING_H__
