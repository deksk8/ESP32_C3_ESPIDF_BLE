// components/ble_server/include/ble_server.h
#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include "esp_err.h"
#include <stdint.h>

// Callbacks para aplicação
typedef void (*ble_on_write_cb_t)(uint8_t *data, uint16_t len);
typedef void (*ble_on_connect_cb_t)(uint16_t conn_handle);
typedef void (*ble_on_disconnect_cb_t)(void);

typedef struct
{
    const char *device_name;
    ble_on_write_cb_t on_write;
    ble_on_connect_cb_t on_connect;
    ble_on_disconnect_cb_t on_disconnect;
} ble_server_config_t;

/**
 * @brief Inicializa servidor BLE
 *
 * @param config Configuração de callbacks
 * @return ESP_OK se sucesso
 */
esp_err_t ble_server_init(const ble_server_config_t *config);

/**
 * @brief Envia notificação para cliente conectado
 *
 * @param data Dados a enviar (máx 512 bytes)
 * @param len Tamanho dos dados
 * @return ESP_OK se enviado com sucesso
 */
esp_err_t ble_server_notify(uint8_t *data, uint16_t len);

/**
 * @brief Atualiza valor da característica de leitura
 *
 * @param value Novo valor
 * @return ESP_OK se atualizado
 */
esp_err_t ble_server_update_read_value(uint32_t value);

#endif