#include "motor_request_queue.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define MOTOR_REQUEST_QUEUE_LENGTH 16

static QueueHandle_t g_motor_request_queue;

esp_err_t motor_request_queue_init(void)
{
    if (g_motor_request_queue != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    g_motor_request_queue = xQueueCreate(MOTOR_REQUEST_QUEUE_LENGTH, sizeof(motor_request_t));
    return g_motor_request_queue != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t motor_request_queue_send(const motor_request_t *request)
{
    if (g_motor_request_queue == NULL || request == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return xQueueSend(g_motor_request_queue, request, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

bool motor_request_queue_receive(motor_request_t *request)
{
    return g_motor_request_queue != NULL && request != NULL &&
           xQueueReceive(g_motor_request_queue, request, 0) == pdTRUE;
}