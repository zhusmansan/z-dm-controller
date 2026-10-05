#ifndef DC_MOTOR_PWM_H
#define DC_MOTOR_PWM_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "motor_config.h"

#define DC_MOTOR_PWM_CHANNEL_COUNT 5

esp_err_t dc_motor_pwm_init(const motor_config_t *configs, size_t config_count);
uint8_t dc_motor_pwm_channel_count(void);
void dc_motor_pwm_set_speed(uint8_t channel, int16_t speed_percent);
void dc_motor_pwm_stop_all(void);

#endif
