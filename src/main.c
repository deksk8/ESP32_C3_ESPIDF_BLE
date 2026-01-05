#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "ble_server.h"
#include "status_led.h"

static const char *TAG = "APP_MAIN";
int contador = 0;

// Callback: Dados recebidos via Write
void on_ble_write(uint8_t *data, uint16_t len)
{
    ESP_LOGI(TAG, "Comando recebido: %.*s", len, data);

    if (strncmp((char *)data, "UNLOCK", 6) == 0)
    {
        ESP_LOGI(TAG, "🔓 Destravando fechadura...");
        status_led_set_color(LED_COLOR_GREEN); // Simples assim!

        // Simula destrave
        vTaskDelay(pdMS_TO_TICKS(500));

        // Atualiza status
        ble_server_update_read_value(1); // 1 = Destravado

        // Envia notificação
        uint8_t status[] = "UNLOCKED";
        ble_server_notify(status, sizeof(status));
    }
    else if (strncmp((char *)data, "LOCK", 4) == 0)
    {
        ESP_LOGI(TAG, "🔒 Travando fechadura...");
        status_led_set_color(LED_COLOR_RED);

        ble_server_update_read_value(0); // 0 = Travado

        uint8_t status[] = "LOCKED";
        ble_server_notify(status, sizeof(status));
    }
}

// Callback: Cliente conectou
void on_ble_connect(uint16_t conn_handle)
{
    ESP_LOGI(TAG, "📱 Cliente conectado: handle=%d", conn_handle);
    status_led_set_color(LED_COLOR_BLUE);
}

// Callback: Cliente desconectou
void on_ble_disconnect(void)
{
    ESP_LOGI(TAG, "📴 Cliente desconectado");
    status_led_set_color(LED_COLOR_OFF);
}

void app_main(void)
{
    // // --- 1. Verificação de Hardware ---
    // esp_chip_info_t chip_info;
    // esp_chip_info(&chip_info);

    // ESP_LOGI(TAG, "Iniciando o sistema embarcado..."); // Agora isso vai aparecer!
    // ESP_LOGI(TAG, "Modelo do Chip: %s", CONFIG_IDF_TARGET);
    // ESP_LOGI(TAG, "Revisão de Silício: %d", chip_info.revision);
    // ESP_LOGI(TAG, "Cores: %d", chip_info.cores);

    // // Verifica o tamanho da Flash
    // uint32_t flash_size = 0;
    // if (esp_flash_get_size(NULL, &flash_size) == ESP_OK)
    // {
    //     // Nota: O formato correto para uint32_t é %lu (unsigned long) ou PRIu32
    //     ESP_LOGI(TAG, "Tamanho da Flash: %luMB", flash_size / (1024 * 1024));
    // }

    // // --- 2. Loop Principal ---
    // for (int i = 1000; i >= 0; i--)
    // {
    //     ESP_LOGI(TAG, "Sistema ativo. Reiniciando em %d segundos...", i);
    //     vTaskDelay(pdMS_TO_TICKS(1000)); // Use pdMS_TO_TICKS para portabilidade
    // }

    // ESP_LOGW(TAG, "Reiniciando agora...");
    // esp_restart();
    // Configuração do servidor BLE
    ble_server_config_t config = {
        .device_name = "SmartLock-ESP32",
        .on_write = on_ble_write,
        .on_connect = on_ble_connect,
        .on_disconnect = on_ble_disconnect,
    };

    // Inicializa servidor
    ESP_ERROR_CHECK(ble_server_init(&config));

    status_led_init();
    status_led_set_color(LED_COLOR_PURPLE);

    ESP_LOGI(TAG, "✅ Sistema pronto!");

    // while (1)
    // {
    //     ESP_LOGI(TAG, "Contador: %d", contador++);
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
}