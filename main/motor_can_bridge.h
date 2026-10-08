#ifndef MOTOR_CAN_BRIDGE_H
#define MOTOR_CAN_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "motor_config.h"

#define MOTOR_CAN_BRIDGE_MAX_MOTORS 8

esp_err_t motor_can_bridge_init(const motor_config_t *configs, size_t config_count);
esp_err_t motor_can_bridge_send(const int16_t motor_speeds[MOTOR_CAN_BRIDGE_MAX_MOTORS]);
uint8_t motor_can_bridge_channel_count(void);

#endif