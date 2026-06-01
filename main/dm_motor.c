#include "dm_motor.h"
#include "math.h"
#include "board_config.h"
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "dm_motor";
#define POLL_DEPTH 10

typedef struct
{
	twai_frame_t frame;
	uint8_t data[8];
} twai_listener_data_t;

typedef struct
{
	twai_node_handle_t node_hdl;
	twai_listener_data_t rx_pool[POLL_DEPTH];
	SemaphoreHandle_t free_pool_semaphore;
	SemaphoreHandle_t rx_result_semaphore;
	int write_idx;
	int read_idx;
} twai_listener_ctx_t;

const Motor_register_def_t register_defs[24] = {
	{.reg_id = 0, .reg_type = REG_TYPE_FLOAT, .is_readonly = false, .shortname = "UV_Value", .description = "Under-voltage protection value (see UNDER_VOLTAGE 0x9)"},
	{.reg_id = 1, .reg_type = REG_TYPE_FLOAT, .is_readonly = false, .shortname = "KT_Value", .description = "Torque coefficient used in MIT mode"},
	{.reg_id = 2, .reg_type = REG_TYPE_FLOAT, .is_readonly = false, .shortname = "OT_Value", .description = "Over-temperature protection value (see MOS/ROTOR over-temp)"},
	{.reg_id = 3, .reg_type = REG_TYPE_FLOAT, .is_readonly = false, .shortname = "OC_Value", .description = "Over-current protection value (see OVER_CURRENT 0xA)"},
	{.reg_id = 4, .reg_type = REG_TYPE_FLOAT, .is_readonly = false, .shortname = "ACC", .description = "Acceleration"},
	{.reg_id = 5, .reg_type = REG_TYPE_FLOAT, .is_readonly = false, .shortname = "DEC", .description = "Deceleration"},
	{.reg_id = 6, .reg_type = REG_TYPE_FLOAT, .is_readonly = false, .shortname = "MAX_SPD", .description = "Maximum speed"},
	{.reg_id = 7, .reg_type = REG_TYPE_UINT32, .is_readonly = false, .shortname = "MST_ID", .description = "Feedback ID"},
	{.reg_id = 8, .reg_type = REG_TYPE_UINT32, .is_readonly = false, .shortname = "ESC_ID", .description = "Receive ID"},
	{.reg_id = 9, .reg_type = REG_TYPE_UINT32, .is_readonly = false, .shortname = "TIMEOUT", .description = "Timeout alarm time (1 register unit = 50 microseconds; see LOST_COMM 0xD)"},
	{.reg_id = 10, .reg_type = REG_TYPE_UINT32, .is_readonly = false, .shortname = "CTRL_MODE", .description = "Control mode"},
	{.reg_id = 11, .reg_type = REG_TYPE_FLOAT, .is_readonly = true, .shortname = "Damp", .description = "Motor viscous damping coefficient"},
	{.reg_id = 12, .reg_type = REG_TYPE_FLOAT, .is_readonly = true, .shortname = "Inertia", .description = "Motor moment of inertia"},
	{.reg_id = 13, .reg_type = REG_TYPE_UINT32, .is_readonly = true, .shortname = "hw_ver", .description = "Reserved"},
	{.reg_id = 14, .reg_type = REG_TYPE_UINT32, .is_readonly = true, .shortname = "sw_ver", .description = "Software version number"},
	{.reg_id = 15, .reg_type = REG_TYPE_UINT32, .is_readonly = true, .shortname = "SN", .description = "Reserved"},
	{.reg_id = 16, .reg_type = REG_TYPE_UINT32, .is_readonly = true, .shortname = "NPP", .description = "Motor pole pairs"},
	{.reg_id = 17, .reg_type = REG_TYPE_FLOAT, .is_readonly = true, .shortname = "Rs", .description = "Motor phase resistance"},
	{.reg_id = 18, .reg_type = REG_TYPE_FLOAT, .is_readonly = true, .shortname = "Ls", .description = "Motor phase inductance"},
	{.reg_id = 19, .reg_type = REG_TYPE_FLOAT, .is_readonly = true, .shortname = "Flux", .description = "Motor flux linkage value"},
	{.reg_id = 20, .reg_type = REG_TYPE_FLOAT, .is_readonly = true, .shortname = "Gr", .description = "Gear reduction ratio"},
	{.reg_id = 21, .reg_type = REG_TYPE_FLOAT, .is_readonly = false, .shortname = "PMAX", .description = "Position mapping range"},
	{.reg_id = 22, .reg_type = REG_TYPE_FLOAT, .is_readonly = false, .shortname = "VMAX", .description = "Speed mapping range"},
	{.reg_id = 23, .reg_type = REG_TYPE_FLOAT, .is_readonly = false, .shortname = "TMAX", .description = "Torque mapping range"}};

// Create semaphore for receive notification
twai_listener_ctx_t can_listener_ctx = {0};

twai_node_handle_t can_node = NULL;

/**
 * int float_to_uint(float x, float x_min, float x_max, int bits)
 *
 * convert a float into int by spanning the range of the variable and
 * linearly assign the value
 *
 * Input:
 * float x: the float variable to be converted into int
 * float x_min: the min value of x
 * float x_max: the max value of x
 * int bits: bit number of converted int
 *
 * Output:
 * int: the converted int
 */
int float_to_uint(float x, float x_min, float x_max, int bits)
{
	float span = x_max - x_min;
	float offset = x_min;
	return (int32_t)((x - offset) * ((float)((1 << bits) - 1)) / span);
}

float uint_to_float(int x_int, float x_min, float x_max, int bits)
{
	/// converts unsigned int to float, given range and number of bits ///
	float span = x_max - x_min;
	float offset = x_min;
	return ((float)x_int) * span / ((float)((1 << bits) - 1)) + offset;
}

// Transmission completion callback
static IRAM_ATTR bool twai_sender_tx_done_callback(twai_node_handle_t handle, const twai_tx_done_event_data_t *edata, void *user_ctx)
{
	if (!edata->is_tx_success)
	{
		ESP_EARLY_LOGW(TAG, "Failed to transmit message, ID: 0x%X", edata->done_tx_frame->header.id);
	}
	return false; // No task wake required
}

// Bus error callback
static IRAM_ATTR bool twai_sender_on_error_callback(twai_node_handle_t handle, const twai_error_event_data_t *edata, void *user_ctx)
{
	ESP_EARLY_LOGW(TAG, "TWAI node error: 0x%x", edata->err_flags.val);
	return false; // No task wake required
}

// Error callback
static bool IRAM_ATTR twai_listener_on_error_callback(twai_node_handle_t handle, const twai_error_event_data_t *edata, void *user_ctx)
{
	ESP_EARLY_LOGW(TAG, "bus error: 0x%x", edata->err_flags.val);
	return false;
}

// Node state
static bool IRAM_ATTR twai_listener_on_state_change_callback(twai_node_handle_t handle, const twai_state_change_event_data_t *edata, void *user_ctx)
{
	const char *twai_state_name[] = {"error_active", "error_warning", "error_passive", "bus_off"};
	ESP_EARLY_LOGI(TAG, "state changed: %s -> %s", twai_state_name[edata->old_sta], twai_state_name[edata->new_sta]);
	return false;
}

// Коллбэк для получения сообщений, вызывается драйвером TWAI при получении сообщения
// Ставит полученные сообщения в кольцевой буфер и уведомляет об этом основную задачу через семафор
// Должно быть достаточно быстрым, чтобы не блокировать прерывания TWAI, поэтому не выполняет никакой обработки сообщений, а просто сохраняет их для основной задачи
static bool IRAM_ATTR twai_listener_rx_callback(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
	BaseType_t woken = pdFALSE;

	twai_listener_ctx_t *ctx = (twai_listener_ctx_t *)user_ctx;

	if (xSemaphoreTakeFromISR(ctx->free_pool_semaphore, &woken) != pdTRUE)
	{
		ESP_EARLY_LOGI(TAG, "Pool full, dropping frame");
		return (woken == pdTRUE);
	}
	if (twai_node_receive_from_isr(handle, &ctx->rx_pool[ctx->write_idx].frame) == ESP_OK)
	{
		ctx->write_idx = (ctx->write_idx + 1) % POLL_DEPTH;
		xSemaphoreGiveFromISR(ctx->rx_result_semaphore, &woken);
	}
	return (woken == pdTRUE);
}

// Инициализация CAN интерфейса, создание TWAI ноды и настройка коллбеков
bool setupCan()
{

	ESP_LOGI(TAG, "setup CAN bus...");

	can_listener_ctx.free_pool_semaphore = xSemaphoreCreateCounting(POLL_DEPTH, POLL_DEPTH);
	can_listener_ctx.rx_result_semaphore = xSemaphoreCreateCounting(POLL_DEPTH, 0);
	assert(can_listener_ctx.free_pool_semaphore != NULL);
	assert(can_listener_ctx.rx_result_semaphore != NULL);

	// can_listener_ctx.rx_pool = calloc(POLL_DEPTH, sizeof(twai_listener_data_t));
	// assert(can_listener_ctx.rx_pool != NULL);
	for (int i = 0; i < POLL_DEPTH; i++)
	{
		can_listener_ctx.rx_pool[i].frame.buffer = can_listener_ctx.rx_pool[i].data;
		can_listener_ctx.rx_pool[i].frame.buffer_len = sizeof(can_listener_ctx.rx_pool[i].data);
	}
	// Configure CAN node
	twai_onchip_node_config_t node_config = {
		.io_cfg = {
			.tx = CAN_TX_GPIO,
			.rx = CAN_RX_GPIO,
			.quanta_clk_out = GPIO_NUM_NC,
			.bus_off_indicator = GPIO_NUM_NC,
		},
		.bit_timing = {
			.bitrate = CAN_BITRATE,
		},
		.fail_retry_cnt = 3,
		.tx_queue_depth = CAN_QUEUE_DEPTH,
	};

	// Create CAN node
	ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &can_node));

	// Register transmission completion callback
	twai_event_callbacks_t callbacks = {
		.on_tx_done = twai_sender_tx_done_callback,
		.on_error = twai_sender_on_error_callback,
		.on_rx_done = twai_listener_rx_callback,
		.on_state_change = twai_listener_on_state_change_callback,
	};
	ESP_ERROR_CHECK(twai_node_register_event_callbacks(can_node, &callbacks, &can_listener_ctx));

	// Enable CAN node
	ESP_ERROR_CHECK(twai_node_enable(can_node));
	ESP_LOGI(TAG, "CAN Sender started successfully");
	return true;
}

void DM_Motor_Init(DM_Motor_t *motor)
{
	for (int i = 0; i < 24; i++)
	{
		motor->registers[i].def = &register_defs[i];
		motor->registers[i].uint_value = 0;
		motor->registers[i].float_value = 0.0f;
	}
}

/**
 * DM_Motor_Send - sends a CAN message to the motor and waits for the response
 * can_id: CAN ID to send to
 * data: pointer to 8 bytes of data to send
 * retPtr: pointer to a variable to store the decoded response, can be NULL if no
 * frameDecoder: pointer to a function to decode the response frame, can be NULL to just print the response
 */
void DM_Motor_Send(uint16_t can_id, uint8_t *data, void *retPtr, void (*frameDecoder)(void *, twai_listener_data_t *))
{

	twai_frame_t tx_msg = {
		.header.id = can_id, // Message ID
		.header.ide = false, // DO NOT Use 29-bit extended ID format
		.buffer = data,		 // Pointer to data to transmit
		.buffer_len = 8,	 // Length of data to transmit
	};

	ESP_ERROR_CHECK(twai_node_transmit(can_node, &tx_msg, 0)); // Timeout = 0: returns immediately if queue is full
	ESP_ERROR_CHECK(twai_node_transmit_wait_all_done(can_node, -1));

	if (xSemaphoreTake(can_listener_ctx.rx_result_semaphore, pdMS_TO_TICKS(15)) == pdTRUE)
	{
		twai_frame_t *frame = &can_listener_ctx.rx_pool[can_listener_ctx.read_idx].frame;

		if (frameDecoder != NULL)
		{
			frameDecoder(retPtr, &can_listener_ctx.rx_pool[can_listener_ctx.read_idx]);
		}
		else
		{
			ESP_LOGI(TAG, "RX: timestamp %llu, %x [%d] %x %x %x %x %x %x %x %x",
					 frame->header.timestamp, frame->header.id, frame->header.dlc,
					 frame->buffer[0], frame->buffer[1], frame->buffer[2], frame->buffer[3],
					 frame->buffer[4], frame->buffer[5], frame->buffer[6], frame->buffer[7]);
		}
		can_listener_ctx.read_idx = (can_listener_ctx.read_idx + 1) % POLL_DEPTH;
		xSemaphoreGive(can_listener_ctx.free_pool_semaphore);
	}
}

/**
 * DM_Motor_Decode_Feedback - decodes a received CAN message into motor state
 * retPtr: pointer to a DM_Motor_t struct to store the decoded state
 * twd: pointer to the received CAN message data
 */
void DM_Motor_Decode_Feedback(void *retPtr, twai_listener_data_t *twd)
{

	DM_Motor_t *motor = (DM_Motor_t *)retPtr;
	// data_frame->id = (frame->buffer[0]) & 0x0F;
	if ((twd->data[0] & 0x0F) == motor->motor_id)
	{
		motor->state.state = (twd->data[0]) >> 4;
		motor->state.pos_int = (twd->data[1] << 8) | twd->data[2];
		motor->state.vel_int = (twd->data[3] << 4) | (twd->data[4] >> 4);
		motor->state.torq_int = ((twd->data[4] & 0xF) << 8) | twd->data[5];
		motor->state.position = uint_to_float(motor->state.pos_int, P_MIN, P_MAX, 16); // (-12.5,12.5)
		motor->state.velocity = uint_to_float(motor->state.vel_int, V_MIN, V_MAX, 12); // (-45.0,45.0)
		motor->state.torque = uint_to_float(motor->state.torq_int, T_MIN, T_MAX, 12);  // (-18.0,18.0)
		motor->state.t_mos = (twd->data[6]);
		motor->state.t_rotor = (twd->data[7]);
	}
}

/**
 * DM_Send_Command - sends a pre-defined command to the motor
 * motor: pointer to the motor struct to send the command to
 * cmd: the command to send
 * The actual command frames are defined in dm_motor.h as CMDS, and this function just looks up the appropriate frame for the given command and sends it using DM_Motor_Send. The response is ignored for commands, so no decoder is provided.
 */
void DM_Send_Command(DM_Motor_t *motor, Motor_Cmd_e cmd)
{
	/* Transmit the pre-defined command frame for this motor */
	DM_Motor_Send(motor->motor_id, (uint8_t *)CMDS[cmd], NULL, NULL);
}

/**
 * DM_Motor_Decode_Register - decodes a received CAN message into a motor register value
 * retPtr: pointer to a Motor_register_t struct to store the decoded register value
 * twd: pointer to the received CAN message data
 */
void DM_Motor_Decode_Register(void *retPtr, twai_listener_data_t *twd)
{

	Motor_register_t *reg = (Motor_register_t *)retPtr;
	if (reg->def->reg_type == REG_TYPE_FLOAT)
	{
		reg->float_value = *((float *)&twd->data[4]);
	}
	else
	{
		reg->uint_value = *((uint32_t *)&twd->data[4]);
	}
}

/**
 * DM_Write_Register - writes a value to a motor register by sending a CAN message
 * motor: pointer to the motor struct to write the register for
 * reg: pointer to the Motor_register_t struct containing the register definition and value to write
 */
void DM_Write_Register(DM_Motor_t *motor, Motor_register_t *reg)
{
	uint8_t *data = motor->tx_buf;

	data[0] = motor->motor_id;
	data[1] = 0;
	data[2] = 0x55;
	data[3] = reg->def->reg_id;
	if (reg->def->reg_type == REG_TYPE_FLOAT)
	{
		*((float *)&data[4]) = reg->float_value;
	}
	else
	{
		*((uint32_t *)&data[4]) = reg->uint_value;
	}

	DM_Motor_Send(0x7FF, motor->tx_buf, reg, DM_Motor_Decode_Register);
}

void DM_Read_Register(DM_Motor_t *motor, Motor_register_t *reg)
{
	uint8_t *data = motor->tx_buf;

	data[0] = motor->motor_id;
	data[1] = 0;
	data[2] = 0x33;
	data[3] = reg->def->reg_id;

	*(uint32_t *)&data[4] = 0;

	// *v = value;

	DM_Motor_Send(0x7FF, motor->tx_buf, reg, DM_Motor_Decode_Register);
}

void DM_Motor_Ctrl_MIT(DM_Motor_t *motor)
{
	uint16_t pos_temp, vel_temp, kp_temp, kd_temp, torq_temp;

	uint8_t *data = motor->tx_buf;

	pos_temp = float_to_uint(motor->cmd_position, P_MIN, P_MAX, 16);
	vel_temp = float_to_uint(motor->cmd_velocity, V_MIN, V_MAX, 12);
	torq_temp = float_to_uint(motor->cmd_torque, T_MIN, T_MAX, 12);

	kp_temp = float_to_uint(motor->cmd_kp, KP_MIN, KP_MAX, 12);
	kd_temp = float_to_uint(motor->cmd_kd, KD_MIN, KD_MAX, 12);

	data[0] = (pos_temp >> 8);
	data[1] = pos_temp;
	data[2] = (vel_temp >> 4);
	data[3] = ((vel_temp & 0xF) << 4) | (kp_temp >> 8);
	data[4] = kp_temp;
	data[5] = (kd_temp >> 4);
	data[6] = ((kd_temp & 0xF) << 4) | (torq_temp >> 8);
	data[7] = torq_temp;
	DM_Motor_Send(motor->motor_id, motor->tx_buf, motor, DM_Motor_Decode_Feedback);
}
