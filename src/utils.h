#ifndef UTILS_H
#define UTILS_H

#include "sensor_types.h"

const char *sensor_type_to_str(enum sensor_type type);
bool validate_message(const struct sensor_message *msg);

#endif
