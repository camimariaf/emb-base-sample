#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "sensor_types.h"
#include "utils.h" 

LOG_MODULE_DECLARE(monit_sys, LOG_LEVEL_INF);

void temperature_producer(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    static const int temp_data_points[] = { 22, 17, 29, 31, 26, 18, 30, 10 };
    uint32_t seq = 1; 
    size_t idx = 0;

    LOG_INF("Producer [TEMP]: iniciado.");

    while (true) {
        struct sensor_message msg = {
            .type = SENSOR_TEMPERATURE,
            .value = temp_data_points[idx],
            .sequence = seq,
        };

        k_msgq_put(&input_msgq, &msg, K_FOREVER);
        LOG_INF("Producer [%s] Leitura #%u: %d°C enviada", 
                sensor_type_to_str(msg.type), seq++, msg.value);

        idx = (idx + 1) % ARRAY_SIZE(temp_data_points);
        k_msleep(800);
    }
}

void humidity_producer(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    static const int umid_data_points[] = { 45, 35, 60, 75, 50, 40, 70, 80 };
    uint32_t seq = 1;
    size_t idx = 0;

    LOG_INF("Producer [HUMID]: iniciado.");

    while (true) {
        struct sensor_message msg = {
            .type = SENSOR_HUMIDITY,
            .value = umid_data_points[idx],
            .sequence = seq,
        };

        k_msgq_put(&input_msgq, &msg, K_FOREVER);
        LOG_INF("Producer [%s] Leitura #%u: %d%% enviada", 
                sensor_type_to_str(msg.type), seq++, msg.value);

        idx = (idx + 1) % ARRAY_SIZE(umid_data_points);
        k_msleep(1000);
    }
}

