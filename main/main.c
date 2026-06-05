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
#include "http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
// #include "display.h"
static const char *TAG = "twai_sender";

// #define MOTOR_LIM_MIN (-M_PI / 4.0)
#define MOTOR_LIM_MAX 135.0/180.0*(M_PI)
#define MOTOR_TORQUE 2.0f

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
    esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();
	wifi_config_t wifi_config = {
		// .ap = {
		// 	.ssid = "DamiaoMotor",
		// 	.ssid_len = strlen("DamiaoMotor"),
		// 	.channel = 1,
		// 	.password = "12345678",
		// 	.max_connection = 4,
		// 	.authmode = WIFI_AUTH_WPA_WPA2_PSK,
		// },
		.sta = {
			.ssid = "Keenetic-8115",
			.password = "PUrYnaMG",
		}
	};

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_ERROR_CHECK(esp_wifi_connect());


    /* Set sta as the default interface */
    esp_netif_set_default_netif(esp_netif_sta);


	ESP_LOGI(TAG, "WiFi AP started. SSID: DamiaoMotor, Password: 12345678");
}

void app_main(void)
{
	// setup_display();
	DM_Motor_t motor;

	memset(&motor, 0, sizeof(DM_Motor_t));
	DM_Motor_Init(&motor);

	motor.motor_id = 0x1;
	motor.feedback_id = motor.motor_id | 0x10;

	setupCan();

	motor.cmd_torque = 0;
	setMotorParameters(&motor);

	for (int i = 0; i < 24; i++)
	{
		vTaskDelay(pdMS_TO_TICKS(1));

		DM_Read_Register(&motor, &motor.registers[i]);
		if (motor.registers[i].def->reg_type == REG_TYPE_FLOAT)
		{
			ESP_LOGI(TAG, "%2.d:%s = %f\t%s", i, motor.registers[i].def->shortname, motor.registers[i].float_value, motor.registers[i].def->description);
		}
		else
		{
			ESP_LOGI(TAG, "%2.d:%s = %d\t%s", i, motor.registers[i].def->shortname, motor.registers[i].uint_value, motor.registers[i].def->description);
		}
	}

	/* Initialize WiFi AP */
	setup_wifi_ap();

	/* Initialize HTTP server with WebSocket support */
	http_server_config_t http_config = {
		.motor = &motor
	};
	http_server_init(&http_config);

	int status_update_counter = 0;

	while (true)
	{
		if (motor.state.state == M_STATE_DISABLED)
		{
			// DM_Send_Command(&motor, M_CMD_ENABLE);
		}
		if (motor.state.state == M_STATE_LOST_COMM)
		{
			// мотор упал в ошибку, пытаемся его реанимировать

			setMotorParameters(&motor);
			DM_Send_Command(&motor, M_CMD_CLEAR_ERROR);
			ESP_LOGI(TAG, "Motor state: %d", motor.state.state);
		}

		if (motor.state.position >= MOTOR_LIM_MAX && motor.cmd_torque > 0)
		{
			motor.cmd_torque = 0;
		}

		if (motor.state.position <= 0 && motor.cmd_torque < 0)
		{
			motor.cmd_torque = 0;
		}

		DM_Motor_Ctrl_MIT(&motor);

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
