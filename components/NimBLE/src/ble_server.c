// components/ble_server/src/ble_server.c
#include "ble_server.h"
#include "esp_log.h"
#include "nvs_flash.h"

// NimBLE
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "BLE_SERVER";

static void ble_app_advertise(void);

// ===== UUIDs (128-bit customizados) =====
// Gerados com: uuidgen (Linux) ou online em uuidgenerator.net

// Service UUID: Lock Control Service
//"12345678-5678-1234-7856-12349abcdef0"
static const ble_uuid128_t gatt_svr_svc_uuid =
    BLE_UUID128_INIT(
        0x23, 0xd1, 0xbc, 0xea, 0x5f, 0x78, 0x23, 0x15,
        0xde, 0xef, 0x12, 0x12, 0x23, 0x15, 0x00, 0x00);

// Characteristic UUID: Command (Write)
static const ble_uuid128_t gatt_svr_chr_cmd_uuid =
    BLE_UUID128_INIT(
        0x23, 0xd1, 0xbc, 0xea, 0x5f, 0x78, 0x23, 0x15,
        0xde, 0xef, 0x12, 0x12, 0x25, 0x15, 0x00, 0x00);

// Characteristic UUID: Status (Read + Notify)
static const ble_uuid128_t gatt_svr_chr_status_uuid =
    BLE_UUID128_INIT(
        0x23, 0xd1, 0xbc, 0xea, 0x5f, 0x78, 0x23, 0x15,
        0xde, 0xef, 0x12, 0x12, 0x24, 0x15, 0x00, 0x00);

static const ble_uuid128_t gatt_svr_chr_datetime_uuid =
    BLE_UUID128_INIT(
        0x23, 0xd1, 0xbc, 0xea, 0x5f, 0x78, 0x23, 0x15,
        0xde, 0xef, 0x12, 0x12, 0x26, 0x15, 0x00, 0x00);

// ===== Estado do Servidor =====
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t status_val_handle;  // Handle da characteristic Status
static uint32_t current_status = 0; // Valor atual do status
static ble_server_config_t server_config;

// ===== Callback: Acesso a Characteristic (Read/Write) =====
static int gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid_t *uuid = ctxt->chr->uuid;
    int rc = 0;

    // Characteristic: Command (Write)
    if (ble_uuid_cmp(uuid, &gatt_svr_chr_cmd_uuid.u) == 0)
    {
        switch (ctxt->op)
        {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            ESP_LOGI(TAG, "Write recebido: %d bytes", ctxt->om->om_len);

            // Copia dados recebidos
            uint8_t data[512];
            uint16_t len = ctxt->om->om_len;
            if (len > sizeof(data))
                len = sizeof(data);

            ble_hs_mbuf_to_flat(ctxt->om, data, len, NULL);

            // Chama callback da aplicação
            if (server_config.on_write)
            {
                server_config.on_write(data, len);
            }

            return 0;

        default:
            ESP_LOGW(TAG, "Operação não suportada: %d", ctxt->op);
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    // Characteristic: Status (Read + Notify)
    if (ble_uuid_cmp(uuid, &gatt_svr_chr_status_uuid.u) == 0)
    {
        switch (ctxt->op)
        {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            ESP_LOGI(TAG, "Read solicitado: status = %lu", current_status);

            rc = os_mbuf_append(ctxt->om, &current_status, sizeof(current_status));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        default:
            ESP_LOGW(TAG, "Operação não suportada: %d", ctxt->op);
            return BLE_ATT_ERR_UNLIKELY;
        }
    }
    if (ble_uuid_cmp(uuid, &gatt_svr_chr_datetime_uuid.u) == 0)
    {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
        {
            uint8_t data[7];
            uint16_t len = ctxt->om->om_len;

            if (len == 7)
            {
                ble_hs_mbuf_to_flat(ctxt->om, data, len, NULL);
                ESP_LOGI(TAG, "Data/Hora: %02d/%02d/20%02d %02d:%02d:%02d",
                         data[2], data[1], data[0], data[3], data[4], data[5]);

                // Aqui você pode chamar uma função para atualizar o relógio (RTC)
                return 0;
            }
            else
            {
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

// ===== Definição dos Serviços GATT =====
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        // Serviço: Lock Control
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                // Characteristic: Command (Write)
                .uuid = &gatt_svr_chr_cmd_uuid.u,
                .access_cb = gatt_svr_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                // Characteristic: Status (Read + Notify)
                .uuid = &gatt_svr_chr_status_uuid.u,
                .access_cb = gatt_svr_chr_access,
                .val_handle = &status_val_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                // Characteristic: Data e Hora (Write)
                .uuid = &gatt_svr_chr_datetime_uuid.u,
                .access_cb = gatt_svr_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {
                0, // Fim da lista de características
            }},
    },
    {
        0, // Fim da lista de serviços
    },
};

// ===== Callback: Eventos GAP (Conexão/Desconexão) =====
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "Cliente conectado: handle=%d, status=%d",
                 event->connect.conn_handle, event->connect.status);

        if (event->connect.status == 0)
        {
            conn_handle = event->connect.conn_handle;

            if (server_config.on_connect)
            {
                server_config.on_connect(conn_handle);
            }
        }
        else
        {
            // Falha na conexão, retoma advertising
            ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                              NULL, ble_gap_event, NULL);
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Cliente desconectado: reason=%d",
                 event->disconnect.reason);

        conn_handle = BLE_HS_CONN_HANDLE_NONE;

        if (server_config.on_disconnect)
        {
            server_config.on_disconnect();
        }

        // Retoma advertising
        ble_app_advertise();
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU atualizado: %d", event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "Notificações %s: conn=%d, attr=%d",
                 event->subscribe.cur_notify ? "habilitadas" : "desabilitadas",
                 event->subscribe.conn_handle,
                 event->subscribe.attr_handle);
        return 0;
    }

    return 0;
}

// ===== Inicialização do Advertising =====
static void ble_app_advertise(void)
{
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields = {0};
    struct ble_hs_adv_fields rsp_fields = {0};
    int rc;

    // === PACOTE 1: ADVERTISING (Obrigatório, pequeno) ===
    // Aqui colocamos o UUID para o app achar a fechadura rápido
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    fields.uuids128 = &gatt_svr_svc_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 0; // Lista completa de UUIDs de 128 bits

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Erro ao configurar dados de advertising: %d", rc);
        return;
    }

    // === PACOTE 2: SCAN RESPONSE (Opcional, solicitado pelo celular) ===
    // Aqui colocamos o Nome, que pode ser longo
    rsp_fields.name = (uint8_t *)server_config.device_name;
    rsp_fields.name_len = strlen(server_config.device_name);
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Erro ao configurar dados de scan response: %d", rc);
        return;
    }

    // === INICIAR O ADVERTISING ===
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    // MELHORIA DE VELOCIDADE:
    // O valor é em unidades de 0.625ms.
    // Mínimo: 32 * 0.625 = 20ms
    // Máximo: 48 * 0.625 = 30ms
    // Isso faz a ESP32 anunciar MUITO rápido.
    adv_params.itvl_min = 32;
    adv_params.itvl_max = 48;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event, NULL);

    if (rc != 0)
    {
        ESP_LOGE(TAG, "Erro ao iniciar advertising: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "Advertising iniciado com sucesso: '%s'", server_config.device_name);
}

// ===== Callback: Stack BLE sincronizado =====
static void ble_app_on_sync(void)
{
    int rc;

    // CORREÇÃO: Criar variável para receber o tipo de endereço
    uint8_t own_addr_type;

    // Passar o endereço da variável (&own_addr_type) em vez de NULL
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Erro ao inferir tipo de endereço: %d", rc);
        return;
    }

    // (Opcional) Usar o tipo inferido se quiser garantir o match
    // Mas geralmente para addr público, o código abaixo já funciona:

    uint8_t addr[6];
    rc = ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, addr, NULL);
    if (rc == 0)
    {
        ESP_LOGI(TAG, "Endereço BLE: %02x:%02x:%02x:%02x:%02x:%02x",
                 addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    }

    ble_app_advertise();
}

// ===== Task do NimBLE =====
static void ble_host_task(void *param)
{
    nimble_port_run(); // Roda até nimble_port_stop()
    nimble_port_freertos_deinit();
}

// ===== API Pública =====

esp_err_t ble_server_init(const ble_server_config_t *config)
{
    if (config == NULL || config->device_name == NULL)
    {
        ESP_LOGE(TAG, "Configuração inválida");
        return ESP_ERR_INVALID_ARG;
    }

    // Copia configuração
    server_config = *config;

    // Inicializa NVS (necessário para BLE)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicializa NimBLE
    ESP_ERROR_CHECK(nimble_port_init());

    // Configura callbacks
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    ble_hs_cfg.reset_cb = NULL; // Poderia adicionar handler de reset

    // Configura nome do dispositivo
    ble_svc_gap_device_name_set(server_config.device_name);

    // Registra serviços GATT
    int rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Erro ao contar serviços: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Erro ao adicionar serviços: %d", rc);
        return ESP_FAIL;
    }

    // Inicia serviço GAP padrão
    ble_svc_gap_init();

    // Inicia serviço GATT padrão
    ble_svc_gatt_init();

    // Cria task do host BLE
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "Servidor BLE inicializado");
    return ESP_OK;
}

esp_err_t ble_server_notify(uint8_t *data, uint16_t len)
{
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE)
    {
        ESP_LOGW(TAG, "Nenhum cliente conectado");
        return ESP_ERR_INVALID_STATE;
    }

    if (len > 512)
    {
        ESP_LOGE(TAG, "Dados muito grandes: %d bytes (máx 512)", len);
        return ESP_ERR_INVALID_SIZE;
    }

    // Cria mbuf com os dados
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL)
    {
        ESP_LOGE(TAG, "Erro ao alocar mbuf");
        return ESP_ERR_NO_MEM;
    }

    // Envia notificação
    int rc = ble_gattc_notify_custom(conn_handle, status_val_handle, om);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Erro ao enviar notificação: %d", rc);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Notificação enviada: %d bytes", len);
    return ESP_OK;
}

esp_err_t ble_server_update_read_value(uint32_t value)
{
    current_status = value;
    ESP_LOGD(TAG, "Valor de leitura atualizado: %lu", value);
    return ESP_OK;
}