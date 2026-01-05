#include "status_led.h"
#include "led_strip.h"

#define LED_GPIO 8

static led_strip_handle_t led_strip;



esp_err_t status_led_init(void)
{
    // Configuração do LED para led_strip v3.x
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
        // Mudança aqui: de led_pixel_format para color_component_format
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .led_model = LED_MODEL_WS2812,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };

    return led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
}

// FUNÇÃO CORRIGIDA (FORA DA INIT)
esp_err_t status_led_set_color(led_status_color_t color)
{
    esp_err_t err;
    switch (color)
    {
    case LED_COLOR_RED:
        err = led_strip_set_pixel(led_strip, 0, 255, 0, 0);
        break;
    case LED_COLOR_GREEN:
        err = led_strip_set_pixel(led_strip, 0, 0, 255, 0);
        break;
    case LED_COLOR_BLUE:
        err = led_strip_set_pixel(led_strip, 0, 0, 0, 255);
        break;
    case LED_COLOR_PURPLE:
        err = led_strip_set_pixel(led_strip, 0, 128, 0, 128);
        break;
    case LED_COLOR_OFF:
        err = led_strip_clear(led_strip);
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    if (err != ESP_OK)
        return err;
    return led_strip_refresh(led_strip); // Aplica a cor
}