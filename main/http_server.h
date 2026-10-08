#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "dm_motor.h"
#include "esp_err.h"

esp_err_t http_server_init(void);
void http_server_broadcast_motor_configs(void);
void http_server_broadcast_status(DM_Motor_t *motor);

#endif
