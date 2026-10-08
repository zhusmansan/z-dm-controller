#include "motor_can_bridge.h"
#include "esp_err.h"
#include "esp_log.h"

#include <string.h>

#include "can_bus.h"
static const char *TAG = "motor_can_bridge";

static uint16_t g_can_id;
static uint8_t g_motor_count;

esp_err_t motor_can_bridge_init(const motor_config_t *configs, size_t config_count)
{
    if (configs == NULL && config_count != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    g_can_id = 0;
    g_motor_count = 0;

    for (size_t i = 0; i < config_count; ++i) {
        if (configs[i].type != MOTOR_TYPE_CAN) {
            continue;
        }
        if (configs[i].can_id > 0x7FF ) {
            return ESP_ERR_INVALID_ARG;
        }
        if (g_motor_count == 0) {
            g_can_id = configs[i].can_id;
        } else if (configs[i].can_id != g_can_id) {
            return ESP_ERR_INVALID_ARG;
        }
        g_motor_count ++;
    }

    ESP_LOGI(TAG, "Motor CAN bridge initialized with %d motors on CAN ID 0x%03X", g_motor_count, g_can_id);

    return ESP_OK;
}

esp_err_t motor_can_bridge_send(const int16_t motor_speeds[MOTOR_CAN_BRIDGE_MAX_MOTORS])
{
    if (motor_speeds == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (g_motor_count == 0) {
        return ESP_OK;//No motors, no problem
    }

    uint8_t payload[MOTOR_CAN_BRIDGE_MAX_MOTORS];
    for (uint8_t i = 0; i < g_motor_count; ++i) {
        if (motor_speeds[i] < -100 || motor_speeds[i] > 100) {
            return ESP_ERR_INVALID_ARG;
        }
        payload[i] = (uint8_t)(int8_t)motor_speeds[i];
    }

    return can_bus_send_frame(g_can_id, payload, g_motor_count);
}

uint8_t motor_can_bridge_channel_count(void)
{
    return g_motor_count;
}