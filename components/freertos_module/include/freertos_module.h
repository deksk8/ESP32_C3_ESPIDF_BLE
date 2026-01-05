// freertos_module.h
#ifndef __FREERTOS_MODULE_H__
#define __FREERTOS_MODULE_H__

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Inicializa o módulo FreeRTOS
     *
     * Esta função cria as tarefas do produtor e consumidor,
     * inicializa o mutex e a fila de mensagens.
     */
    void freertos_module_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __FREERTOS_MODULE_H__ */