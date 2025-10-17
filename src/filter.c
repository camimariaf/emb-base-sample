#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "sensor_types.h"
#include "utils.h" 

LOG_MODULE_DECLARE(monit_sys, LOG_LEVEL_INF);

void filter_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    uint32_t accepted_count = 0;
    uint32_t rejected_count = 0;

    LOG_INF("FILTER: Filtro de Validação: iniciado");

    while (true) {
        struct sensor_message msg;
        k_msgq_get(&input_msgq, &msg, K_FOREVER);

        const char *unit = (msg.type == SENSOR_TEMPERATURE) ? "°C" : "%%";

        if (validate_message(&msg)) {
            k_msgq_put(&output_msgq, &msg, K_FOREVER);
            accepted_count++;
            LOG_INF("FILTER [%s] ✓ %s: %d%s ACEITO [total=%u]", 
                    sensor_type_to_str(msg.type), sensor_type_to_str(msg.type), 
                    msg.value, unit, accepted_count);
        } else {
            rejected_count++;
            LOG_WRN("FILTER [%s] ✗ %s: %d%s REJEITADO (fora dos limites) [total=%u]", 
                    sensor_type_to_str(msg.type), sensor_type_to_str(msg.type), 
                    msg.value, unit, rejected_count);
        }
    }
}
