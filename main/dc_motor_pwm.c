#include "dc_motor_pwm.h"

#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
static const char *TAG = "dc_motor_pwm";

#define PWM_FREQ_HZ 2000
#define PWM_STEPS 100
#define PWM_PERIOD_US (1000000ULL / (PWM_FREQ_HZ * PWM_STEPS))

typedef struct {
    uint8_t duty_percent;
    bool reverse;
    bool enabled;
    bool configured;
} pwm_state;

static const motor_config_t *g_channels[DC_MOTOR_PWM_CHANNEL_COUNT];
static pwm_state g_pwm_states[DC_MOTOR_PWM_CHANNEL_COUNT];
static uint8_t g_channel_count = 0;

static esp_timer_handle_t g_pwm_timer = NULL;
static uint8_t g_pwm_phase = 0;
static bool g_initialized = false;

static void pwm_timer_callback(void *arg)
{
    (void)arg;

    g_pwm_phase = (g_pwm_phase + 1) % PWM_STEPS;

    for (size_t i = 0; i < g_channel_count; ++i) {
        const motor_config_t *motor = g_channels[i];
        pwm_state *pwm = &g_pwm_states[i];

        if (!pwm->configured) {
            continue;
        }
        if (!pwm->enabled || pwm->duty_percent == 0) {
            gpio_set_level(motor->pwm_pins.forward_gpio, 0);
            gpio_set_level(motor->pwm_pins.reverse_gpio, 0);
            continue;
        }

        bool active = g_pwm_phase < pwm->duty_percent;
        if (pwm->reverse) {
            gpio_set_level(motor->pwm_pins.forward_gpio, 0);
            gpio_set_level(motor->pwm_pins.reverse_gpio, active ? 1 : 0);
        } else {
            gpio_set_level(motor->pwm_pins.forward_gpio, active ? 1 : 0);
            gpio_set_level(motor->pwm_pins.reverse_gpio, 0);
        }
    }
}

static void configure_channel(const motor_config_t *channel, pwm_state *pwm)
{
    *pwm = (pwm_state){
        .duty_percent = 0,
        .reverse = false,
        .enabled = false,
        .configured = true,
    };

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << channel->pwm_pins.forward_gpio) | (1ULL << channel->pwm_pins.reverse_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(channel->pwm_pins.forward_gpio, 0);
    gpio_set_level(channel->pwm_pins.reverse_gpio, 0);
}

esp_err_t dc_motor_pwm_init(const motor_config_t *configs, size_t config_count)
{
    if (g_initialized) {
        return ESP_OK;
    }
    if (configs == NULL || config_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < config_count; ++i) {
        if (configs[i].type != MOTOR_TYPE_PWM_HBRIDGE) {
            continue;
        }
        if (g_channel_count >= DC_MOTOR_PWM_CHANNEL_COUNT) {
            return ESP_ERR_INVALID_SIZE;
        }
        ESP_LOGI(TAG, "Configuring motor %d of type %d", configs[i].id, configs[i].type);
        g_channels[g_channel_count] = &configs[i];
        configure_channel(g_channels[g_channel_count], &g_pwm_states[g_channel_count]);
        ++g_channel_count;
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

void dc_motor_pwm_set_speed(const motor_config_t *motor, int16_t speed_percent)
{
    if (motor == NULL || motor->type != MOTOR_TYPE_PWM_HBRIDGE) {
        ESP_LOGW(TAG, "dc_motor_pwm_set_speed: Invalid pwm channel");
        return;
    }

    if (!g_initialized) {
        ESP_LOGE(TAG, "PWM driver is not initialized");
        return;
    }

    pwm_state *pwm = NULL;
    for (size_t i = 0; i < g_channel_count; ++i) {
        if (g_channels[i] == motor) {
            pwm = &g_pwm_states[i];
            break;
        }
    }
    if (pwm == NULL) {
        ESP_LOGW(TAG, "dc_motor_pwm_set_speed: PWM channel is not configured");
        return;
    }

    if (speed_percent > 100) {
        speed_percent = 100;
    } else if (speed_percent < -100) {
        speed_percent = -100;
    }

    if (speed_percent == 0) {
        pwm->enabled = false;
        pwm->duty_percent = 0;
        gpio_set_level(motor->pwm_pins.forward_gpio, 0);
        gpio_set_level(motor->pwm_pins.reverse_gpio, 0);
        return;
    }

    pwm->reverse = speed_percent < 0;
    pwm->duty_percent = (uint8_t)(abs(speed_percent));
    pwm->enabled = true;
}

void dc_motor_pwm_stop_all(void)
{
    for (size_t i = 0; i < g_channel_count; ++i) {
        if (g_channels[i] == NULL || !g_pwm_states[i].configured) {
            continue;
        }
        pwm_state *pwm = &g_pwm_states[i];
        pwm->enabled = false;
        pwm->duty_percent = 0;
        gpio_set_level(g_channels[i]->pwm_pins.forward_gpio, 0);
        gpio_set_level(g_channels[i]->pwm_pins.reverse_gpio, 0);
    }
}
