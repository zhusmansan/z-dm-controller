#ifndef MOTOR_CONFIG_H
#define MOTOR_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"

typedef enum {
    MOTOR_TYPE_DAMIAO_CAN,
    MOTOR_TYPE_PWM_HBRIDGE,
    MOTOR_TYPE_CAN
} motor_type_t;

typedef struct {
    gpio_num_t forward_gpio;
    gpio_num_t reverse_gpio;
} motor_pwm_pin_config_t;

typedef struct {
    motor_type_t type;
    uint8_t id;
    uint16_t can_id;
    motor_pwm_pin_config_t pwm_pins;
} motor_config_t;

extern const motor_config_t motor_configs[];
extern const size_t motor_config_count;

const motor_config_t *motor_config_find(motor_type_t type, size_t index);
const motor_config_t *motor_config_find_by_id(motor_type_t type, uint8_t id);
size_t motor_config_channel_count(void);

#endif