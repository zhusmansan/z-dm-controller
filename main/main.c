/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include "dm_motor.h"
#include "dc_motor_pwm.h"
#include "motor_control.h"
#include "motor_can_bridge.h"
#include "motor_request_queue.h"
#include "motor_command_interface.h"
#include "http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "board_config.h"

// #include "display.h"
static const char *TAG = "motor_controller_main";

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

	ESP_ERROR_CHECK(gpio_config(&io_config));
	
	gpio_set_drive_capability(CAN_GND_GPIO, GPIO_DRIVE_CAP_3);
	gpio_set_drive_capability(CAN_VCC_GPIO, GPIO_DRIVE_CAP_3);

	gpio_set_level(CAN_GND_GPIO, 0);
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
	
	ESP_LOGE(TAG, "1");
	
	ESP_ERROR_CHECK(motor_request_queue_init());
	ESP_LOGE(TAG, "2");
	if (motor_config_find(MOTOR_TYPE_PWM_HBRIDGE, 0) != NULL) {
		ESP_ERROR_CHECK(dc_motor_pwm_init(motor_configs, motor_config_count));
	}
	ESP_LOGE(TAG, "3");
	ESP_ERROR_CHECK(motor_can_bridge_init(motor_configs, motor_config_count));
	ESP_LOGE(TAG, "4");
	setupCan();
	ESP_LOGE(TAG, "5");

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

	/* Initialize HTTP server with WebSocket support */
	ESP_ERROR_CHECK(http_server_init());
	ESP_ERROR_CHECK(motor_control_start(&motor));

	int status_update_counter = 0;

	while (true)
	{
		http_server_broadcast_status(&motor);
	
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}
