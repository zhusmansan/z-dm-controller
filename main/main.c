/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/param.h>
#include "dm_motor.h"
#include "dc_motor_pwm.h"
#include "motor_can_bridge.h"
#include "http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "board_config.h"

// #include "display.h"
static const char *TAG = "twai_sender";

// #define MOTOR_LIM_MIN (-M_PI / 4.0)
#define MOTOR_LIM_MAX 135.0/180.0*(M_PI)
#define MOTOR_TORQUE 2.0f

static esp_err_t send_dm_motor_command(void *context, motor_command_t command)
{
	DM_Motor_t *motor = context;
	Motor_Cmd_e dm_command;

	switch (command)
	{
	case MOTOR_COMMAND_ENABLE:
		dm_command = M_CMD_ENABLE;
		break;
	case MOTOR_COMMAND_DISABLE:
		dm_command = M_CMD_DISABLE;
		break;
	case MOTOR_COMMAND_CLEAR_ERROR:
		dm_command = M_CMD_CLEAR_ERROR;
		break;
	case MOTOR_COMMAND_SET_ZERO_POSITION:
		dm_command = M_CMD_SET_ZERO_POSITION;
		break;
	default:
		return ESP_ERR_INVALID_ARG;
	}

	DM_Send_Command(motor, dm_command);
	return ESP_OK;
}

static esp_err_t set_configured_motor_speed(void *context, uint8_t channel, int16_t speed)
{
	(void)context;
	if (motor_config_find_by_id(MOTOR_TYPE_CAN, channel) != NULL) {
		return motor_can_bridge_set_speed(channel, speed);
	}
	if (motor_config_find_by_id(MOTOR_TYPE_PWM_HBRIDGE, channel) != NULL) {
		dc_motor_pwm_set_speed(channel, speed);
		return ESP_OK;
	}
	return ESP_ERR_INVALID_ARG;
}

static esp_err_t stop_all_configured_motors(void *context)
{
	(void)context;
	dc_motor_pwm_stop_all();
	if (motor_can_bridge_channel_count() > 0) {
		return motor_can_bridge_stop_all();
	}
	return ESP_OK;
}

void setMotorParameters(DM_Motor_t *motor)
{
	// Режим управления MIT
	motor->registers[10].uint_value = M_CONTROL_MODE_MIT;
	DM_Write_Register(motor, &motor->registers[10]);

	// Коэффициент крутящего момента (Kt) для MIT режима
	// Если его не установить, не будет работать переданный feed_forward крутящий момент
	motor->registers[1].float_value = 4;
	DM_Write_Register(motor, &motor->registers[1]);
	
	// Устанавливаем таймаут
	// Если в течение этого времени не отправлять данные, мотор выдаст ошибку LOST_COMM (0xD)
	motor->registers[9].uint_value = MOTOR_TIMEOUT_MS * (1000 / 50);
	DM_Write_Register(motor, &motor->registers[9]);
}

static void setup_wifi_ap(void)
{
	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	esp_netif_create_default_wifi_ap();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_netif_create_default_wifi_sta();
	wifi_config_t wifi_config = {
		.ap = {
			.ssid = "Roboarm",
			.ssid_len = strlen("Roboarm"),
			.channel = 1,
			.password = "12345678",
			.max_connection = 4,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK,
		},
		// .sta = {
		// 	.ssid = "Keenetic-8115",
		// 	.password = "PUrYnaMG",
		// }
	};

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	// ESP_ERROR_CHECK(esp_wifi_connect());


    /* Set sta as the default interface */
    // esp_netif_set_default_netif(esp_netif_sta);


	ESP_LOGI(TAG, "WiFi AP started. SSID: DamiaoMotor, Password: 12345678");
}

void app_main(void)
{
		const uint64_t pin_mask = (1ULL << CAN_GND_GPIO) | (1ULL << CAN_VCC_GPIO);
	gpio_config_t io_config = {
		.pin_bit_mask = pin_mask,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};

	esp_err_t err = gpio_config(&io_config);
	
	err = gpio_set_level(CAN_GND_GPIO, 0);
	
	gpio_set_level(CAN_VCC_GPIO, 1);
	// setup_display();
	DM_Motor_t motor;

	memset(&motor, 0, sizeof(DM_Motor_t));
	// DM_Motor_Init(&motor);

	const motor_config_t *dm_config = motor_config_find(MOTOR_TYPE_DAMIAO_CAN, 0);
	if (dm_config == NULL) {
		ESP_LOGE(TAG, "Damiao motor configuration is missing");
		// return;
	}else{
		motor.motor_id = dm_config->id;
		motor.feedback_id = motor.motor_id | 0x10;
	}
	setupCan();
	ESP_ERROR_CHECK(motor_can_bridge_init(motor_configs, motor_config_count));
	if (motor_config_find(MOTOR_TYPE_PWM_HBRIDGE, 0) != NULL) {
		ESP_ERROR_CHECK(dc_motor_pwm_init(motor_configs, motor_config_count));
	}

	// motor.cmd_torque = 0;
	// setMotorParameters(&motor);

	// for (int i = 0; i < 24; i++)
	// {
	// 	vTaskDelay(pdMS_TO_TICKS(1));

	// 	DM_Read_Register(&motor, &motor.registers[i]);
	// 	if (motor.registers[i].def->reg_type == REG_TYPE_FLOAT)
	// 	{
	// 		ESP_LOGI(TAG, "%2.d:%s = %f\t%s", i, motor.registers[i].def->shortname, motor.registers[i].float_value, motor.registers[i].def->description);
	// 	}
	// 	else
	// 	{
	// 		ESP_LOGI(TAG, "%2.d:%s = %d\t%s", i, motor.registers[i].def->shortname, motor.registers[i].uint_value, motor.registers[i].def->description);
	// 	}
	// }

	/* Initialize WiFi AP */
	setup_wifi_ap();

	motor_command_interface_t motor_commands = {
		.context = &motor,
		.send_command = send_dm_motor_command,
		.motor_context = NULL,
		.motor_channel_count = motor_config_channel_count(),
		.set_motor_speed = set_configured_motor_speed,
		.stop_all_motors = stop_all_configured_motors,
	};

	/* Initialize HTTP server with WebSocket support */
	http_server_config_t http_config = {
		.motor = &motor,
		.motor_commands = motor_commands,
	};
	http_server_init(&http_config);

	int status_update_counter = 0;
	int pwm_phase = 0;

	while (true)
	{
		if (motor.state.state == M_STATE_DISABLED)
		{
			// motor_commands.send_command(motor_commands.context, MOTOR_COMMAND_ENABLE);
		}
		if (motor.state.state == M_STATE_LOST_COMM)
		{
			// мотор упал в ошибку, пытаемся его реанимировать

			setMotorParameters(&motor);
			// motor_commands.send_command(motor_commands.context, MOTOR_COMMAND_CLEAR_ERROR);
			// ESP_LOGI(TAG, "Motor state: %d", motor.state.state);
		}

		if (motor.state.position >= MOTOR_LIM_MAX && motor.cmd_torque > 0)
		{
			motor.cmd_torque = 0;
		}

		if (motor.state.position <= 0 && motor.cmd_torque < 0)
		{
			motor.cmd_torque = 0;
		}

		// DM_Motor_Ctrl_MIT(&motor);

		// for (uint8_t ch = 0; ch < DC_MOTOR_PWM_CHANNEL_COUNT; ++ch)
		// {
		// 	int16_t speed = ((pwm_phase + ch) % 2 == 0) ? 80 : -80;
		// 	dc_motor_pwm_set_speed(ch, speed);
		// }
		// pwm_phase = (pwm_phase + 1) % 4;

		/* Broadcast motor status to WebSocket clients every 50ms */
		status_update_counter++;
		if (status_update_counter >= 5)
		{
			http_server_broadcast_status(&motor);
			status_update_counter = 0;
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}
