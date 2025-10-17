#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#define STACK_SIZE 1024
#define PRIORITY 7

enum sensor_type {
    SENSOR_TEMPERATURE = 0,
    SENSOR_HUMIDITY = 1,
};

struct sensor_message {
    enum sensor_type type;
    int value;
    uint32_t sequence;
};

extern struct k_msgq input_msgq;
extern struct k_msgq output_msgq;

#endif
