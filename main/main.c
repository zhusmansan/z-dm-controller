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
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
// #include "display.h"
static const char *TAG = "twai_sender";

#define MOTOR_LIM_MIN (-M_PI / 4.0)
#define MOTOR_LIM_MAX (M_PI / 4.0)
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


void app_main(void)
{
	// setup_display();
	DM_Motor_t motor;

	memset(&motor, 0, sizeof(DM_Motor_t));
	DM_Motor_Init(&motor);

	motor.motor_id = 0x1;
	motor.feedback_id = motor.motor_id | 0x10;

	setupCan();

	motor.cmd_torque = MOTOR_TORQUE;
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

	while (true)
	{
		if (motor.state.state == M_STATE_DISABLED)
		{
			DM_Send_Command(&motor, M_CMD_ENABLE);
		}
		if (motor.state.state == M_STATE_LOST_COMM)
		{
			// мотор упал в ошибку, пытаемся его реанимировать

			setMotorParameters(&motor);
			DM_Send_Command(&motor, M_CMD_CLEAR_ERROR);
			ESP_LOGI(TAG, "Motor state: %d", motor.state.state);
		}

		if (motor.state.position >= MOTOR_LIM_MAX)
		{
			motor.cmd_torque = -MOTOR_TORQUE;
		}

		if (motor.state.position <= MOTOR_LIM_MIN)
		{
			motor.cmd_torque = MOTOR_TORQUE;
		}

		DM_Motor_Ctrl_MIT(&motor);

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}
