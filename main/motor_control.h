#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "dm_motor.h"
#include "esp_err.h"

esp_err_t motor_control_start(DM_Motor_t *motor);

#endif