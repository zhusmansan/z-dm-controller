#ifndef DM_MOTOR_H
#define DM_MOTOR_H

#include <stdint.h>
#include <stdbool.h>
// // #include "bsp_can.h"

#define MOTOR_TIMEOUT_MS 50

#define P_MIN -12.5f
#define P_MAX 12.5f
#define V_MIN -10.0f
#define V_MAX 10.0f
#define KP_MIN 0.0f
#define KP_MAX 500.0f
#define KD_MIN 0.0f
#define KD_MAX 5.0f
#define T_MIN -28.0f
#define T_MAX 28.0f

#define DM_MOTOR_MIT (0x0)
#define DM_MOTOR_POS_VEL (0x100)
#define DM_MOTOR_VEL (0x200)
#define DM_MOTOR_FORCE_POS (0x300)

// #define DM_MOTOR_ZERO_CURRENT (0)
// #define DM_MOTOR_HARDWARE_DISABLE (1)

#define M_STATE_DISABLED 0x0
#define M_STATE_ENABLED 0x1
#define M_STATE_OVER_VOLTAGE 0x8
#define M_STATE_UNDER_VOLTAGE 0x9
#define M_STATE_OVER_CURRENT 0xA
#define M_STATE_MOS_OVER_TEMP 0xB
#define M_STATE_ROTOR_OVER_TEMP 0xC
#define M_STATE_LOST_COMM 0xD
#define M_STATE_OVERLOAD 0xE

typedef enum
{
	M_CMD_ENABLE,
	M_CMD_DISABLE,
	M_CMD_SET_ZERO_POSITION,
	M_CMD_CLEAR_ERROR
} Motor_Cmd_e;

static uint8_t CMDS[4][8] = {
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC},
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFD},
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE},
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFB}};

typedef struct
{
	uint8_t state;
	uint16_t pos_int;
	uint16_t vel_int;
	uint16_t torq_int;

	float position;
	float velocity;
	float torque;
	float t_mos;
	float t_rotor;

} DM_Motor_State_t;

typedef enum
{
	M_CONTROL_MODE_MIT = 1,
	M_CONTROL_MODE_POS_VEL = 2,
	M_CONTROL_MODE_VEL = 3,
	M_CONTROL_MODE_FORCE_POS = 4
} Motor_Control_mode_e;

typedef enum
{
	REG_TYPE_FLOAT,
	REG_TYPE_UINT32
} Reg_Type_e;

typedef struct
{
	uint8_t reg_id;
	Reg_Type_e reg_type;
	bool is_readonly;
	const char *shortname;
	const char *description;
} Motor_register_def_t;

typedef struct
{
	const Motor_register_def_t *def;
	uint32_t uint_value;
	float float_value;
} Motor_register_t;

extern const Motor_register_def_t register_defs[24];

typedef struct
{
	/* CAN Information */
	uint8_t motor_id;		 // (ESC_ID, reg 8)
	uint8_t feedback_id; // (MST_ID, reg 7)

	Motor_Control_mode_e control_mode; // (CTRL_MODE, reg 10) Control mode

	uint8_t send_pending_flag;

	// CAN_Instance_t *can_instance;

	/* Motor Target */
	float cmd_position;
	float cmd_velocity;
	float cmd_torque;
	float cmd_kp;
	float cmd_kd;

	DM_Motor_State_t state;

	uint8_t tx_buf[8];
	uint8_t rx_buf[8];
	Motor_register_t registers[24];

} DM_Motor_t;

// // void DM_Motor_Disable_Motor(DM_Motor_Handle_t *motor);
// // void DM_Motor_Enable_Motor(DM_Motor_Handle_t *motor);
// // DM_Motor_Handle_t* DM_Motor_Init(DM_Motor_Config_t *config);
// // void DM_Motor_Ctrl_MIT(DM_Motor_Handle_t *motor, float target_pos, float target_vel, float torque);
// // void DM_Motor_Set_MIT_PD(DM_Motor_Handle_t *motor, float cmd_kp, float cmd_kd);

// /**
//  * Global function to send the motor control data
// */
// void DM_Motor_Send(void);
void DM_Motor_Vel(float target_vel, uint8_t *data);
bool setupCan();
void DM_Motor_Init(DM_Motor_t *motor);
void DM_Send_Command(DM_Motor_t *motor, Motor_Cmd_e cmd);
void DM_Motor_Ctrl_MIT(DM_Motor_t *motor);
void DM_Write_Register(DM_Motor_t *motor, Motor_register_t *reg);
void DM_Read_Register(DM_Motor_t *motor, Motor_register_t *reg);
#endif