#include "freertos_module.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "FRTOS_MOD";

// 1. Recurso Compartilhado e Mutex
static int shared_counter = 0;
static SemaphoreHandle_t shared_mutex = NULL;

// 2. Fila de Comunicação
static QueueHandle_t message_queue = NULL;

// Estrutura da mensagem a ser enviada pela fila
typedef struct
{
    int value;
    char description[32];
} message_t;

// Tarefa 1: Produtor (Acessa o recurso compartilhado e envia mensagem)
static void producer_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Produtor iniciado.");

    while (1)
    {
        // Protege o acesso ao shared_counter com o Mutex
        if (xSemaphoreTake(shared_mutex, portMAX_DELAY) == pdTRUE)
        {
            shared_counter++;
            ESP_LOGI(TAG, "Contador compartilhado incrementado para: %d", shared_counter);
            xSemaphoreGive(shared_mutex); // Libera o Mutex
        }

        // Envia uma mensagem pela fila
        message_t msg;
        msg.value = shared_counter;
        strncpy(msg.description, "Novo valor do contador", sizeof(msg.description) - 1);
        msg.description[sizeof(msg.description) - 1] = '\0';

        if (xQueueSend(message_queue, &msg, 0) != pdPASS)
        {
            ESP_LOGW(TAG, "Fila cheia, mensagem descartada.");
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Tarefa 2: Consumidor (Lê o recurso compartilhado e processa a mensagem)
static void consumer_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Consumidor iniciado.");
    message_t received_msg;

    while (1)
    {
        // Tenta receber uma mensagem da fila (espera por 100ms)
        if (xQueueReceive(message_queue, &received_msg, pdMS_TO_TICKS(100)) == pdPASS)
        {
            ESP_LOGI(TAG, "Mensagem recebida: %s, Valor: %d", received_msg.description, received_msg.value);
        }

        // Acessa o recurso compartilhado (apenas para leitura, mas ainda protegido)
        if (xSemaphoreTake(shared_mutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            ESP_LOGI(TAG, "Consumidor leu o contador: %d", shared_counter);
            xSemaphoreGive(shared_mutex);
        }
        else
        {
            ESP_LOGW(TAG, "Não foi possível obter o Mutex a tempo.");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Função de inicialização do módulo (chamada em app_main)
void freertos_module_init(void)
{
    // Cria o mutex
    shared_mutex = xSemaphoreCreateMutex();
    if (shared_mutex == NULL)
    {
        ESP_LOGE(TAG, "Falha ao criar Mutex!");
        return;
    }

    // Cria a fila
    message_queue = xQueueCreate(10, sizeof(message_t));
    if (message_queue == NULL)
    {
        ESP_LOGE(TAG, "Falha ao criar Queue!");
        vSemaphoreDelete(shared_mutex);
        return;
    }

    ESP_LOGI(TAG, "Inicializando módulo FreeRTOS...");

    // Criação da tarefa Produtor
    BaseType_t ret = xTaskCreate(producer_task,
                                 "Producer",
                                 4096,
                                 NULL,
                                 5,
                                 NULL);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Falha ao criar tarefa do produtor!");
    }

    // Criação da tarefa Consumidor
    ret = xTaskCreate(consumer_task,
                      "Consumer",
                      4096,
                      NULL,
                      4,
                      NULL);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Falha ao criar tarefa do consumidor!");
    }

    ESP_LOGI(TAG, "Módulo FreeRTOS inicializado com sucesso!");
}