#ifndef MOTOR_COMMAND_INTERFACE_H
#define MOTOR_COMMAND_INTERFACE_H

#include <stdint.h>

#include "esp_err.h"

typedef enum {
    MOTOR_COMMAND_ENABLE,
    MOTOR_COMMAND_DISABLE,
    MOTOR_COMMAND_CLEAR_ERROR,
    MOTOR_COMMAND_SET_ZERO_POSITION
} motor_command_t;

typedef struct {
    void *context;
    esp_err_t (*send_command)(void *context, motor_command_t command);
    void *motor_context;
    uint8_t motor_channel_count;
    esp_err_t (*set_motor_speed)(void *context, uint8_t channel, int16_t speed);
    esp_err_t (*stop_all_motors)(void *context);
} motor_command_interface_t;

#endif