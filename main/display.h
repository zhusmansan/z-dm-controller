#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_DISPLAY_LCD_CONTROLLER_SSD1306 1
#define DISPLAY_LCD_PIXEL_CLOCK_HZ    (400 * 1000)
#define DISPLAY_PIN_NUM_SDA           5
#define DISPLAY_PIN_NUM_SCL           6
#define DISPLAY_PIN_NUM_RST           -1
#define DISPLAY_I2C_HW_ADDR           0x3C
#define I2C_HOST  0

#define DISPLAY_LCD_H_RES              128
#define DISPLAY_LCD_V_RES              40

// Bit number used to represent command and parameter
#define DISPLAY_LCD_CMD_BITS           8
#define DISPLAY_LCD_PARAM_BITS         8

typedef enum {
    DISP_CMD_SET_MODE,
    DISP_CMD_UPDATE_TM,
    DISP_CMD_RESET_AVG
} display_cmd_t;

typedef struct {
    display_cmd_t cmd;
    int64_t arg;
} display_event_t;

bool send_to_display_queue(display_cmd_t cmd, int64_t arg);
void update_display(display_cmd_t cmd, int64_t arg);
void display_update_task(void *args);
void setup_display();
void reset_avg_time();
/*  */
#ifdef __cplusplus
}
#endif
