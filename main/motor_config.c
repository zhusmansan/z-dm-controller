#include "motor_config.h"

const motor_config_t motor_configs[] = {
    {.type = MOTOR_TYPE_DAMIAO_CAN, .id = 0, .can_id = 0x07 , .name = "Локоть"},
    {.type = MOTOR_TYPE_CAN, .id = 1, .can_id = 0x100, .name = "Палец 1"},
    {.type = MOTOR_TYPE_CAN, .id = 2, .can_id = 0x100, .name = "Палец 2"},
    {.type = MOTOR_TYPE_CAN, .id = 3, .can_id = 0x100, .name = "Палец 3"},
    {.type = MOTOR_TYPE_CAN, .id = 4, .can_id = 0x100, .name = "Палец 4"},
    {.type = MOTOR_TYPE_PWM_HBRIDGE, .id = 5, .pwm_pins = {.forward_gpio = GPIO_NUM_20, .reverse_gpio = GPIO_NUM_21}, .name = "Вращение кисти"},
};

const size_t motor_config_count = sizeof(motor_configs) / sizeof(motor_configs[0]);

const motor_config_t *motor_config_find(motor_type_t type, size_t index)
{
    for (size_t i = 0; i < motor_config_count; ++i)
    {
        if (motor_configs[i].type == type)
        {
            if (index == 0)
            {
                return &motor_configs[i];
            }
            --index;
        }
    }

    return NULL;
}

const motor_config_t *motor_config_find_by_id(motor_type_t type, uint8_t id)
{
    for (size_t i = 0; i < motor_config_count; ++i)
    {
        if (motor_configs[i].type == type && motor_configs[i].id == id)
        {
            return &motor_configs[i];
        }
    }

    return NULL;
}

size_t motor_config_channel_count(void)
{
    size_t channel_count = 0;

    for (size_t i = 0; i < motor_config_count; ++i)
    {
        if (motor_configs[i].type == MOTOR_TYPE_DAMIAO_CAN ||
            motor_configs[i].type == MOTOR_TYPE_PWM_HBRIDGE ||
            motor_configs[i].type == MOTOR_TYPE_CAN)
        {
            size_t channel_end = (size_t)motor_configs[i].id + 1;
            if (channel_end > channel_count)
            {
                channel_count = channel_end;
            }
        }
    }

    return channel_count;
}