#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <stdint.h>  //Para definir realmente tipos inteiros com tamanhos fixos
#include "esp_err.h" //Para códigos de erro ESP32

typedef enum
{
    LED_COLOR_RED,
    LED_COLOR_GREEN,
    LED_COLOR_BLUE,
    LED_COLOR_PURPLE,
    LED_COLOR_OFF
} led_status_color_t;

esp_err_t status_led_init(void);
esp_err_t status_led_set_color(led_status_color_t color);

#endif // STATUS_LED_H