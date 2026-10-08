#ifndef MOTOR_REQUEST_QUEUE_H
#define MOTOR_REQUEST_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "motor_command_interface.h"

typedef enum {
    MOTOR_REQUEST_COMMAND,
    MOTOR_REQUEST_SET_SPEED,
    MOTOR_REQUEST_STOP_ALL,
    MOTOR_REQUEST_SET_TORQUE,
    MOTOR_REQUEST_SET_KP,
    MOTOR_REQUEST_SET_KD
} motor_request_type_t;

typedef struct {
    motor_request_type_t type;
    motor_command_t command;
    uint8_t channel;
    int16_t speed;
    float value;
} motor_request_t;

esp_err_t motor_request_queue_init(void);
esp_err_t motor_request_queue_send(const motor_request_t *request);
bool motor_request_queue_receive(motor_request_t *request);

#endif