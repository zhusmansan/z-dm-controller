idf_component_register(
    SRCS "main.c" "dm_motor.c" "http_server.c" "dc_motor_pwm.c" "motor_config.c" "motor_can_bridge.c" "motor_request_queue.c" "motor_control.c"
	PRIV_REQUIRES esp_http_server esp_wifi nvs_flash esp_eth
    REQUIRES esp_driver_twai esp_driver_ledc esp_timer esp_http_server esp_driver_gpio esp_wifi esp_event esp_netif nvs_flash esp_eth cjson
    INCLUDE_DIRS "."
    EMBED_FILES "web/index.html" "web/styles.css" "web/app.js"
)
