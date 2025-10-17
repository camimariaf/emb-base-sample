#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "sensor_types.h"
#include "utils.h" 

LOG_MODULE_DECLARE(monit_sys, LOG_LEVEL_INF);

void consumer_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    LOG_INF("Consumer [STORAGE]: Consumidor de Armazenamento: iniciado");

    while (true) {
        struct sensor_message msg;
        k_msgq_get(&output_msgq, &msg, K_FOREVER);

        const char *unit = (msg.type == SENSOR_TEMPERATURE) ? "°C" : "%%";

        k_msleep(200);

        LOG_INF("Consumer [STORAGE] Armazenando %s: %d%s | Latência: 200ms", 
                sensor_type_to_str(msg.type), msg.value, unit);
    }
}

