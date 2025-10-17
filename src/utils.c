#include "utils.h"

const char *sensor_type_to_str(enum sensor_type type)
{
    switch (type) {
    case SENSOR_TEMPERATURE:
        return "TEMP";
    case SENSOR_HUMIDITY:
        return "HUMID";
    default:
        return "UNKNOWN";
    }
}

bool validate_message(const struct sensor_message *msg)
{
    if (msg->type == SENSOR_TEMPERATURE) {
        return (msg->value >= 18) && (msg->value <= 30);
    }
    if (msg->type == SENSOR_HUMIDITY) {
        return (msg->value >= 40) && (msg->value <= 70);
    }
    return false;
}

