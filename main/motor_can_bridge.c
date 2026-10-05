#include "motor_can_bridge.h"

#include <string.h>

#include "can_bus.h"

#define MOTOR_CAN_BRIDGE_MAX_MOTORS 8

static uint16_t g_can_id;
static uint8_t g_motor_count;
static int16_t g_motor_speeds[MOTOR_CAN_BRIDGE_MAX_MOTORS];

esp_err_t motor_can_bridge_init(const motor_config_t *configs, size_t config_count)
{
    if (configs == NULL && config_count != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    g_can_id = 0;
    g_motor_count = 0;
    memset(g_motor_speeds, 0, sizeof(g_motor_speeds));

    for (size_t i = 0; i < config_count; ++i) {
        if (configs[i].type != MOTOR_TYPE_CAN) {
            continue;
        }
        if (configs[i].can_id > 0x7FF || configs[i].id >= MOTOR_CAN_BRIDGE_MAX_MOTORS) {
            return ESP_ERR_INVALID_ARG;
        }
        if (g_motor_count == 0) {
            g_can_id = configs[i].can_id;
        } else if (configs[i].can_id != g_can_id) {
            return ESP_ERR_INVALID_ARG;
        }
        if (configs[i].id >= g_motor_count) {
            g_motor_count = configs[i].id + 1;
        }
    }

    if (g_motor_count == 0) {
        return ESP_OK;
    }

    for (uint8_t channel = 0; channel < g_motor_count; ++channel) {
        bool found = false;
        for (size_t i = 0; i < config_count; ++i) {
            if (configs[i].type == MOTOR_TYPE_CAN && configs[i].id == channel) {
                found = true;
                break;
            }
        }
        if (!found) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    return ESP_OK;
}

static esp_err_t send_speeds(void)
{
    if (g_motor_count == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t payload[MOTOR_CAN_BRIDGE_MAX_MOTORS];
    for (uint8_t i = 0; i < g_motor_count; ++i) {
        payload[i] = (uint8_t)(int8_t)g_motor_speeds[i];
    }

    return can_bus_send_frame(g_can_id, payload, g_motor_count);
}

esp_err_t motor_can_bridge_set_speed(uint8_t channel, int16_t speed)
{
    if (channel >= g_motor_count || speed < -100 || speed > 100) {
        return ESP_ERR_INVALID_ARG;
    }

    g_motor_speeds[channel] = speed;
    return send_speeds();
}

esp_err_t motor_can_bridge_stop_all(void)
{
    memset(g_motor_speeds, 0, sizeof(g_motor_speeds));
    return send_speeds();
}

uint8_t motor_can_bridge_channel_count(void)
{
    return g_motor_count;
}