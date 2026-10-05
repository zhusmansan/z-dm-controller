#ifndef CAN_BUS_H
#define CAN_BUS_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t can_bus_send_frame(uint16_t can_id, const uint8_t *data, size_t length);

#endif