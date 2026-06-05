idf_component_register(
    SRCS "main.c" "dm_motor.c" "http_server.c"
    REQUIRES esp_driver_twai esp_timer esp_http_server esp_wifi esp_event esp_netif nvs_flash esp_eth cjson
    INCLUDE_DIRS "."
    EMBED_FILES "web/index.html" "web/styles.css" "web/app.js"
)
