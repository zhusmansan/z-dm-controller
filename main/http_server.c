#include "http_server.h"
#include "esp_netif.h"
#include "esp_eth.h"

#include <esp_http_server.h>
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "http_server";
static httpd_handle_t server = NULL;
static DM_Motor_t *g_motor = NULL;
static SemaphoreHandle_t g_motor_mutex = NULL;
static int g_ws_fd = -1;

#define MAX_WS_PAYLOAD 512

/* Root handler to serve index.html */
static esp_err_t root_handler(httpd_req_t *req)
{
    extern const unsigned char index_html_start[] asm("_binary_index_html_start");
    extern const unsigned char index_html_end[] asm("_binary_index_html_end");
    const size_t index_html_size = (index_html_end - index_html_start);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, index_html_size);
    return ESP_OK;
}

/* CSS handler */
static esp_err_t css_handler(httpd_req_t *req)
{
    extern const unsigned char styles_css_start[] asm("_binary_styles_css_start");
    extern const unsigned char styles_css_end[] asm("_binary_styles_css_end");
    const size_t styles_css_size = (styles_css_end - styles_css_start);

    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char *)styles_css_start, styles_css_size);
    return ESP_OK;
}

static esp_err_t ws_post_handshake_cb(httpd_req_t *req)
{
    ESP_LOGI(TAG, "=== ws_post_handshake_cb called ===");

    g_ws_fd = httpd_req_to_sockfd(req);
    return ESP_OK;
}


/* JavaScript handler */
static esp_err_t js_handler(httpd_req_t *req)
{
    extern const unsigned char app_js_start[] asm("_binary_app_js_start");
    extern const unsigned char app_js_end[] asm("_binary_app_js_end");
    const size_t app_js_size = (app_js_end - app_js_start);

    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)app_js_start, app_js_size);
    return ESP_OK;
}

/* WebSocket handler */
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == (HTTP_GET ||HTTPD_WS_TYPE_PING)) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        g_ws_fd = httpd_req_to_sockfd(req);
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t buf[MAX_WS_PAYLOAD] = {0};
    
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = buf;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, MAX_WS_PAYLOAD);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
        return ret;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        buf[ws_pkt.len] = '\0';
        cJSON *root = cJSON_Parse((char *)buf);
        
        if (root == NULL) {
            ESP_LOGW(TAG, "Failed to parse JSON");
            cJSON_Delete(root);
            return ESP_OK;
        }

        cJSON *type_item = cJSON_GetObjectItem(root, "type");
        if (type_item == NULL || type_item->type != cJSON_String) {
            ESP_LOGW(TAG, "Missing or invalid 'type' field");
            cJSON_Delete(root);
            return ESP_OK;
        }

        const char *type = type_item->valuestring;

        if (xSemaphoreTake(g_motor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (strcmp(type, "control") == 0) {
                cJSON *action = cJSON_GetObjectItem(root, "action");
                cJSON *value = cJSON_GetObjectItem(root, "value");

                if (action && action->type == cJSON_String && value && value->type == cJSON_Number) {
                     if (strcmp(action->valuestring, "torque") == 0) {
                        g_motor->cmd_torque = value->valuedouble;
                        ESP_LOGI(TAG, "Torque set to %.2f", g_motor->cmd_torque);
                    } else if (strcmp(action->valuestring, "kp") == 0) {
                        g_motor->cmd_kp = value->valuedouble;
                        ESP_LOGI(TAG, "KP set to %.2f", g_motor->cmd_kp);
                    } else if (strcmp(action->valuestring, "kd") == 0) {
                        g_motor->cmd_kd = value->valuedouble;
                        ESP_LOGI(TAG, "KD set to %.2f", g_motor->cmd_kd);
                    }
                }
            } else if (strcmp(type, "command") == 0) {
                cJSON *action = cJSON_GetObjectItem(root, "action");
                if (action && action->type == cJSON_String) {
                    if (strcmp(action->valuestring, "enable") == 0) {
                        if (g_motor->state.state == M_STATE_DISABLED) {
                            DM_Send_Command(g_motor, M_CMD_ENABLE);
                            ESP_LOGI(TAG, "Motor enabled");
                        }
                    } else if (strcmp(action->valuestring, "disable") == 0) {
                        if (g_motor->state.state != M_STATE_DISABLED) {
                            DM_Send_Command(g_motor, M_CMD_DISABLE);
                            ESP_LOGI(TAG, "Motor disabled");
                        }
                    } else if (strcmp(action->valuestring, "clear_error") == 0) {
                        DM_Send_Command(g_motor, M_CMD_CLEAR_ERROR);
                        ESP_LOGI(TAG, "Clear error command sent");
                    } else if (strcmp(action->valuestring, "set_zero") == 0) {
                        DM_Send_Command(g_motor, M_CMD_SET_ZERO_POSITION);
                        ESP_LOGI(TAG, "Set zero position command sent");
                    }
                }
            }

            xSemaphoreGive(g_motor_mutex);
        }

        cJSON_Delete(root);
    } else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGI(TAG, "WebSocket connection closed");
        g_ws_fd = -1;
    }

    return ESP_OK;
}

void http_server_broadcast_status(DM_Motor_t *motor)
{
    if (server == NULL || g_ws_fd < 0) {
        return;
    }

    if (xSemaphoreTake(g_motor_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "status");
    cJSON_AddNumberToObject(root, "state", motor->state.state);
    cJSON_AddNumberToObject(root, "position", motor->state.position);
    cJSON_AddNumberToObject(root, "velocity", motor->state.velocity);
    cJSON_AddNumberToObject(root, "torque", motor->state.torque);
    cJSON_AddNumberToObject(root, "t_mos", motor->state.t_mos);
    cJSON_AddNumberToObject(root, "t_rotor", motor->state.t_rotor);

    char *payload = cJSON_Print(root);
    cJSON_Delete(root);

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)payload;
    ws_pkt.len = strlen(payload);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_send_frame_async(server, g_ws_fd, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "Failed to send WebSocket frame: %s", esp_err_to_name(ret));
        g_ws_fd = -1;
    }

    free(payload);
    xSemaphoreGive(g_motor_mutex);
}

esp_err_t http_server_init(http_server_config_t *config)
{
    if (server != NULL) {
        ESP_LOGE(TAG, "Server already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    g_motor = config->motor;
    g_motor_mutex = xSemaphoreCreateMutex();
    if (g_motor_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create motor mutex");
        return ESP_ERR_NO_MEM;
    }

    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t ret = httpd_start(&server, &http_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ret;
    }

    /* Register URI handlers */
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &root_uri);

    httpd_uri_t css_uri = {
        .uri = "/styles.css",
        .method = HTTP_GET,
        .handler = css_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &css_uri);

    httpd_uri_t js_uri = {
        .uri = "/app.js",
        .method = HTTP_GET,
        .handler = js_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &js_uri);

    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true,
        .ws_post_handshake_cb = ws_post_handshake_cb,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ws_uri);

    ESP_LOGI(TAG, "HTTP server initialized successfully");
    return ESP_OK;
}
