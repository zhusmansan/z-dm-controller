#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "dm_motor.h"
#include "esp_err.h"
typedef struct {
    DM_Motor_t *motor;
} http_server_config_t;

esp_err_t http_server_init(http_server_config_t *config);
void http_server_broadcast_status(DM_Motor_t *motor);

#endif
