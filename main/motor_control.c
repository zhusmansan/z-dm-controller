#include "motor_control.h"

#include <string.h>

#include "dc_motor_pwm.h"
#include "motor_can_bridge.h"
#include "motor_command_interface.h"
#include "motor_config.h"
#include "motor_request_queue.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "motor_control";

#define MOTOR_LIM_MAX (135.0f / 180.0f * 3.14159265358979323846f)

typedef struct {
	int16_t speed;
} local_motor_state_t;

typedef struct {
	DM_Motor_t *motor;
	local_motor_state_t motor_states[MOTOR_CAN_BRIDGE_MAX_MOTORS];
	motor_command_interface_t motor_commands;
} motor_control_context_t;

static motor_control_context_t g_motor_control;

static esp_err_t send_dm_motor_command(void *context, motor_command_t command)
{
	DM_Motor_t *motor = context;
	Motor_Cmd_e dm_command;

	switch (command) {
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
	motor_control_context_t *control = context;
	if (control == NULL || channel >= motor_config_count || speed < -100 || speed > 100) {
		return ESP_ERR_INVALID_ARG;
	}

	const motor_config_t *motor_config = &motor_configs[channel];
	control->motor_states[channel].speed = speed;

	if (motor_config->type == MOTOR_TYPE_PWM_HBRIDGE) {
		dc_motor_pwm_set_speed(motor_config, speed);
		return ESP_OK;
	}

	const motor_config_t *damiao_config = motor_config_find_by_id(MOTOR_TYPE_DAMIAO_CAN, channel);
	if (damiao_config != NULL && control->motor != NULL &&
		control->motor->motor_id == damiao_config->id) {
		control->motor->cmd_velocity = ((float)speed / 100.0f) * V_MAX;
	}

	return ESP_OK;
}

static esp_err_t stop_all_configured_motors(void *context)
{
	motor_control_context_t *control = context;
	if (control == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	memset(control->motor_states, 0, sizeof(control->motor_states));
	if (control->motor != NULL) {
		control->motor->cmd_velocity = 0.0f;
	}
	dc_motor_pwm_stop_all();
	return ESP_OK;
}

static void process_motor_request(const motor_request_t *request)
{
	DM_Motor_t *motor = g_motor_control.motor;
	const motor_command_interface_t *commands = &g_motor_control.motor_commands;
	esp_err_t ret = ESP_OK;

	ESP_LOGI(TAG, "Processing motor request: type=%d, command=%d, channel=%d, speed=%d, value=%f",
			 request->type, request->command, request->channel, request->speed, request->value);

	switch (request->type) {
	case MOTOR_REQUEST_COMMAND:
		if (motor_config_find(MOTOR_TYPE_DAMIAO_CAN, 0) == NULL) {
			ESP_LOGW(TAG, "Ignoring Damiao command: no Damiao motor is configured");
			return;
		}
		if (request->command == MOTOR_COMMAND_ENABLE && motor->state.state != M_STATE_DISABLED) {
			return;
		}
		if (request->command == MOTOR_COMMAND_DISABLE && motor->state.state == M_STATE_DISABLED) {
			return;
		}
		ret = commands->send_command(commands->context, request->command);
		break;
	case MOTOR_REQUEST_SET_SPEED:
		if (request->channel >= motor_config_count) {
			ESP_LOGW(TAG, "Ignoring invalid motor channel %u", request->channel);
			return;
		}
		ret = commands->set_motor_speed(commands->motor_context, request->channel, request->speed);
		break;
	case MOTOR_REQUEST_STOP_ALL:
		ret = commands->stop_all_motors(commands->motor_context);
		break;
	case MOTOR_REQUEST_SET_TORQUE:
		motor->cmd_torque = request->value;
		return;
	case MOTOR_REQUEST_SET_KP:
		motor->cmd_kp = request->value;
		return;
	case MOTOR_REQUEST_SET_KD:
		motor->cmd_kd = request->value;
		return;
	default:
		ESP_LOGW(TAG, "Ignoring unknown motor request type %d", request->type);
		return;
	}

	if (ret != ESP_OK) {
		ESP_LOGW(TAG, "Motor request failed: %s", esp_err_to_name(ret));
	}
}

static void set_motor_parameters(DM_Motor_t *motor)
{
	motor->registers[10].uint_value = M_CONTROL_MODE_MIT;
	DM_Write_Register(motor, &motor->registers[10]);

	motor->registers[1].float_value = 4;
	DM_Write_Register(motor, &motor->registers[1]);

	motor->registers[9].uint_value = MOTOR_TIMEOUT_MS * (1000 / 8);
	DM_Write_Register(motor, &motor->registers[9]);
}

static void motor_control_task(void *argument)
{
	motor_control_context_t *control = argument;
	while (true) {
		motor_request_t request;
		while (motor_request_queue_receive(&request)) {
			process_motor_request(&request);
		}

		if (motor_can_bridge_channel_count() > 0) {
			int16_t motor_speeds[MOTOR_CAN_BRIDGE_MAX_MOTORS] = {0};
			for (uint8_t channel = 0; channel < MOTOR_CAN_BRIDGE_MAX_MOTORS; ++channel) {
				motor_speeds[channel] = control->motor_states[channel].speed;
			}
			esp_err_t ret = motor_can_bridge_send(motor_speeds);
			if (ret != ESP_OK) {
				ESP_LOGD(TAG, "CAN bridge transmit failed: %s", esp_err_to_name(ret));
			}
		}

		if (motor_config_find(MOTOR_TYPE_DAMIAO_CAN, 0) != NULL) {
			DM_Motor_Ctrl_MIT(control->motor);
		}
		if (control->motor->state.state == M_STATE_LOST_COMM) {
			set_motor_parameters(control->motor);
		}

		if (control->motor->state.position >= MOTOR_LIM_MAX && control->motor->cmd_torque > 0) {
			control->motor->cmd_torque = 0;
		}
		if (control->motor->state.position <= 0 && control->motor->cmd_torque < 0) {
			control->motor->cmd_torque = 0;
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

esp_err_t motor_control_start(DM_Motor_t *motor)
{
	if (motor == NULL) {
		return ESP_ERR_INVALID_ARG;
	}
	if (g_motor_control.motor != NULL) {
		return ESP_ERR_INVALID_STATE;
	}

	g_motor_control.motor = motor;
	g_motor_control.motor_commands = (motor_command_interface_t){
		.context = motor,
		.send_command = send_dm_motor_command,
		.motor_context = &g_motor_control,
		.motor_channel_count = motor_config_channel_count(),
		.set_motor_speed = set_configured_motor_speed,
		.stop_all_motors = stop_all_configured_motors,
	};

	if (xTaskCreate(motor_control_task, "motor_control", 4096, &g_motor_control,
				 tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		g_motor_control.motor = NULL;
		return ESP_FAIL;
	}

	return ESP_OK;
}