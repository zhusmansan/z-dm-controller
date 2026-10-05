#ifndef MOTOR_CAN_BRIDGE_H
#define MOTOR_CAN_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "motor_config.h"

esp_err_t motor_can_bridge_init(const motor_config_t *configs, size_t config_count);
esp_err_t motor_can_bridge_set_speed(uint8_t channel, int16_t speed);
esp_err_t motor_can_bridge_stop_all(void);
uint8_t motor_can_bridge_channel_count(void);

#endif