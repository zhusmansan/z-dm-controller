#include "dc_motor_pwm.h"

#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

static const char *TAG = "dc_motor_pwm";

#define PWM_FREQ_HZ 2000
#define PWM_STEPS 100
#define PWM_PERIOD_US (1000000ULL / (PWM_FREQ_HZ * PWM_STEPS))

typedef struct {
    gpio_num_t pwm_gpio_forward;
    gpio_num_t pwm_gpio_reverse;
    uint8_t duty_percent;
    bool reverse;
    bool enabled;
    bool configured;
} dc_motor_pwm_channel_t;

static dc_motor_pwm_channel_t g_channels[DC_MOTOR_PWM_CHANNEL_COUNT];
static esp_timer_handle_t g_pwm_timer = NULL;
static uint8_t g_pwm_phase = 0;
static uint8_t g_channel_count = 0;
static bool g_initialized = false;

static void pwm_timer_callback(void *arg)
{
    (void)arg;

    g_pwm_phase = (g_pwm_phase + 1) % PWM_STEPS;

    for (size_t i = 0; i < g_channel_count; ++i) {
        dc_motor_pwm_channel_t *motor = &g_channels[i];

        if (!motor->configured) {
            continue;
        }
        if (!motor->enabled || motor->duty_percent == 0) {
            gpio_set_level(motor->pwm_gpio_forward, 0);
            gpio_set_level(motor->pwm_gpio_reverse, 0);
            continue;
        }

        bool active = g_pwm_phase < motor->duty_percent;
        if (motor->reverse) {
            gpio_set_level(motor->pwm_gpio_forward, 0);
            gpio_set_level(motor->pwm_gpio_reverse, active ? 1 : 0);
        } else {
            gpio_set_level(motor->pwm_gpio_forward, active ? 1 : 0);
            gpio_set_level(motor->pwm_gpio_reverse, 0);
        }
    }
}

static void configure_channel(dc_motor_pwm_channel_t *channel, gpio_num_t forward_gpio, gpio_num_t reverse_gpio)
{
    channel->pwm_gpio_forward = forward_gpio;
    channel->pwm_gpio_reverse = reverse_gpio;
    channel->duty_percent = 0;
    channel->reverse = false;
    channel->enabled = false;
    channel->configured = true;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << forward_gpio) | (1ULL << reverse_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(forward_gpio, 0);
    gpio_set_level(reverse_gpio, 0);
}

esp_err_t dc_motor_pwm_init(const motor_config_t *configs, size_t config_count)
{
    if (g_initialized) {
        return ESP_OK;
    }
    if (configs == NULL || config_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(g_channels, 0, sizeof(g_channels));
    for (size_t i = 0; i < config_count; ++i) {
        ESP_LOGI(TAG, "Configuring motor %d of type %d", configs[i].id, configs[i].type);
        if (configs[i].type != MOTOR_TYPE_PWM_HBRIDGE) {
            continue;
        }
        if (configs[i].id >= DC_MOTOR_PWM_CHANNEL_COUNT) {
            return ESP_ERR_INVALID_SIZE;
        }
        if (g_channels[configs[i].id].configured) {
            return ESP_ERR_INVALID_ARG;
        }
        configure_channel(&g_channels[configs[i].id], configs[i].pwm_pins.forward_gpio, configs[i].pwm_pins.reverse_gpio);
        if (configs[i].id + 1 > g_channel_count) {
            g_channel_count = configs[i].id + 1;
        }
    }
    if (g_channel_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_timer_create_args_t timer_args = {
        .callback = pwm_timer_callback,
        .arg = NULL,
        .name = "dc_motor_pwm",
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &g_pwm_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(g_pwm_timer, PWM_PERIOD_US));

    g_initialized = true;
    ESP_LOGI(TAG, "PWM driver initialized for %d channels", g_channel_count);
    return ESP_OK;
}

uint8_t dc_motor_pwm_channel_count(void)
{
    return g_channel_count;
}

void dc_motor_pwm_set_speed(uint8_t channel, int16_t speed_percent)
{
    if (channel >= g_channel_count || !g_channels[channel].configured) {
        ESP_LOGW(TAG, "Invalid channel %u", channel);
        return;
    }

    dc_motor_pwm_channel_t *motor = &g_channels[channel];
    if (!g_initialized) {
        ESP_LOGE(TAG, "PWM driver is not initialized");
        return;
    }

    if (speed_percent > 100) {
        speed_percent = 100;
    } else if (speed_percent < -100) {
        speed_percent = -100;
    }

    if (speed_percent == 0) {
        motor->enabled = false;
        motor->duty_percent = 0;
        gpio_set_level(motor->pwm_gpio_forward, 0);
        gpio_set_level(motor->pwm_gpio_reverse, 0);
        return;
    }

    motor->reverse = speed_percent < 0;
    motor->duty_percent = (uint8_t)(abs(speed_percent));
    motor->enabled = true;
}

void dc_motor_pwm_stop_all(void)
{
    for (size_t i = 0; i < g_channel_count; ++i) {
        if (!g_channels[i].configured) {
            continue;
        }
        g_channels[i].enabled = false;
        g_channels[i].duty_percent = 0;
        gpio_set_level(g_channels[i].pwm_gpio_forward, 0);
        gpio_set_level(g_channels[i].pwm_gpio_reverse, 0);
    }
}
