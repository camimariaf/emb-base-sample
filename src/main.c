#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "sensor_types.h"

void temperature_producer(void *arg1, void *arg2, void *arg3);
void humidity_producer(void *arg1, void *arg2, void *arg3);
void filter_thread(void *arg1, void *arg2, void *arg3);
void consumer_thread(void *arg1, void *arg2, void *arg3);

LOG_MODULE_REGISTER(monit_sys, LOG_LEVEL_INF);

K_MSGQ_DEFINE(input_msgq,            // Nome da fila
              sizeof(struct sensor_message), // Tamanho do item
              16,                        // Número máximo de itens
              4);                        // Alinhamento

// Fila de saída: FILTER (dados válidos) -> Consumer
K_MSGQ_DEFINE(output_msgq,
              sizeof(struct sensor_message),
              16,
              4);


K_THREAD_DEFINE(temp_producer_tid, 
                STACK_SIZE, 
                temperature_producer, 
                NULL, NULL, NULL, 
                PRIORITY, 
                0, 
                0);

K_THREAD_DEFINE(humid_producer_tid, 
                STACK_SIZE, 
                humidity_producer, 
                NULL, NULL, NULL, 
                PRIORITY, 
                0, 
                0);

K_THREAD_DEFINE(filter_tid, 
                STACK_SIZE, 
                filter_thread, 
                NULL, NULL, NULL, 
                PRIORITY, 
                0, 
                0);

K_THREAD_DEFINE(consumer_tid, 
                STACK_SIZE, 
                consumer_thread, 
                NULL, NULL, NULL, 
                PRIORITY, 
                0, 
                0);


int main(void)
{
    LOG_INF("===================================================================");
    LOG_INF("|| Sistema de Monitoramento Ambiental - v1.0                     ||");
    LOG_INF("===================================================================");
    LOG_INF("Limite de Validação:");
    LOG_INF("-> Temperatura: 18 a 30°C");
    LOG_INF("-> Umidade: 40 a 70%%");
    LOG_INF("-------------------------------------------------------------------");
    LOG_INF("Pipeline: Producers -> Fila Entrada -> FILTER -> Fila Saída -> Consumer");
    LOG_INF("Sistema em Operação...");
    LOG_INF("-------------------------------------------------------------------");

    return 0;
}
