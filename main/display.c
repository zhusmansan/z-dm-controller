#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_panel_ssd1306.h"

#include "display.h"

static const char *TAG = "DISPLAY";

static QueueHandle_t display_queue = NULL;
static esp_lcd_panel_handle_t lcd_handle;

static _lock_t display_queue_lock; // Mutex for thread safety

static _lock_t lvgl_api_lock;

IRAM_ATTR int64_t time_summ = 0;
IRAM_ATTR int64_t time_count = 0;

lv_obj_t *mode_label = NULL;
lv_obj_t *time_label = NULL;
lv_obj_t *avg_time_label = NULL;
lv_obj_t *distance_label = NULL;
lv_obj_t *avg_distance_label = NULL;

// Display-related functions
int mode = 0;



void update_display(display_cmd_t cmd, int64_t arg)
{
    _lock_acquire(&lvgl_api_lock);

    lv_obj_t *scr = lv_disp_get_scr_act(NULL);

    if (!time_label)
    {
        time_label = lv_label_create(scr);
        lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, 5, 20);
        lv_label_set_text_static(time_label, "Time: 0 us");
    }
    if (!distance_label)
    {
        distance_label = lv_label_create(scr);
        lv_obj_align(distance_label, LV_ALIGN_TOP_LEFT, 5, 40);
        lv_label_set_text_static(distance_label, "Dist: 0 cm");
    }
    if (!avg_distance_label)
    {
        avg_distance_label = lv_label_create(scr);
        lv_obj_align(avg_distance_label, LV_ALIGN_TOP_LEFT, 5, 60);
        lv_label_set_text_static(avg_distance_label, "AvgD: 0 cm");
    }

    if (!mode_label)
    {
        mode_label = lv_label_create(scr);
        lv_obj_align(mode_label, LV_ALIGN_TOP_LEFT, 5, 100);
        switch (mode)
        {
        case 1:
            lv_label_set_text_static(mode_label, "Mode: TX");
            break;
        
        default:
            lv_label_set_text_static(mode_label, "Mode: RX");
            break;
        }
    }

    switch (cmd)
    {
    case DISP_CMD_UPDATE_TM:
        time_summ += arg;
        time_count ++;
        lv_label_set_text_fmt(time_label, "Time: %lld us", arg);

        float distance = ((arg - 2285) / 1000000.0) * 100 * 346.0; // Calculate distance
        lv_label_set_text_fmt(distance_label, "Dist: %d cm", (int)distance);

        float avg_distance = ((time_count == 0 ? 0 : ((float)time_summ / time_count) - 2285) / 1000000.0) * 100 * 346; // Calculate average distance
        lv_label_set_text_fmt(avg_distance_label, "AvgD: %d cm", (int)avg_distance);

        break;
    case DISP_CMD_SET_MODE:
        mode = arg;
        switch (mode)
        {
        case 1:
            lv_label_set_text_static(mode_label, "Mode: TX");
            break;
        
        default:
            lv_label_set_text_static(mode_label, "Mode: RX");
            break;
        }
        break;
    case DISP_CMD_RESET_AVG:
        time_summ = 0;
        time_count = 0;
        break;
    default:
        break;
    }

    _lock_release(&lvgl_api_lock);
}

void display_update_task(void *args)
{
    display_event_t disp_cmd;
    while (1)
    {
        printf("display_queue loop\n");
        if (display_queue != NULL && xQueueReceive(display_queue, &disp_cmd, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            update_display(disp_cmd.cmd, disp_cmd.arg);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    // printf("display_update_task\n");
}

void reset_avg_time()
{
    time_summ = 0;
    time_count = 0;
    send_to_display_queue(DISP_CMD_RESET_AVG, 0);
}

// Function to add events to the display queue (thread-safe)
bool send_to_display_queue(display_cmd_t cmd, int64_t arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (display_queue == NULL)
    {
        return false; // Queue not initialized
    }
    display_event_t disp_evt = {cmd, arg};

    bool result = xQueueSend(display_queue, &disp_evt, 10) == pdTRUE;

    return true;
}

// Display setup function
// void setup_display()
// {

//     gpio_config_t bckl_config = {
//         .pin_bit_mask = (1ULL << PIN_NUM_BCKL),
//         .mode = GPIO_MODE_OUTPUT,
//         .pull_up_en = GPIO_PULLUP_DISABLE,
//         .pull_down_en = GPIO_PULLDOWN_ENABLE,
//         .intr_type = GPIO_INTR_DISABLE};
//     gpio_config(&bckl_config);
//     gpio_set_level(PIN_NUM_BCKL, 1);

//     spi_bus_config_t bus_config = {
//         .sclk_io_num = PIN_NUM_CLK,
//         .mosi_io_num = PIN_NUM_MOSI,
//         .miso_io_num = -1,
//         .quadwp_io_num = -1,
//         .quadhd_io_num = -1,
//         .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t)};
//     spi_bus_initialize(LCD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO);

//     esp_lcd_panel_io_handle_t io_handle = NULL;
//     esp_lcd_panel_io_spi_config_t io_config = {
//         .dc_gpio_num = PIN_NUM_DC,
//         .cs_gpio_num = PIN_NUM_CS,
//         .pclk_hz = LCD_PIXEL_CLOCK_HZ,
//         .flags = {
//             .lsb_first = false,
//         },
//         .lcd_cmd_bits = 8,
//         .lcd_param_bits = 8,
//         .spi_mode = 0,
//         .trans_queue_depth = 10,
//         .flags = {.lsb_first = false}};
//     esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_config, &io_handle);

//     esp_lcd_panel_dev_config_t panel_config = {
//         .reset_gpio_num = PIN_NUM_RST,
//         .rgb_ele_order = LCD_RGB_ENDIAN_RGB,
//         .bits_per_pixel = 16,
//     };
//     esp_lcd_new_panel_st7789(io_handle, &panel_config, &lcd_handle);

//     esp_lcd_panel_reset(lcd_handle);
//     esp_lcd_panel_disp_on_off(lcd_handle, true);
//     esp_lcd_panel_init(lcd_handle);
//     esp_lcd_panel_invert_color(lcd_handle, true);

//     const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
//     lvgl_port_init(&lvgl_cfg);

//     const lvgl_port_display_cfg_t disp_cfg = {
//         .io_handle = io_handle,
//         .panel_handle = lcd_handle,
//         .buffer_size = LCD_H_RES * LCD_V_RES,
//         .double_buffer = false,
//         .hres = LCD_H_RES,
//         .vres = LCD_V_RES,
//         .monochrome = false,
//         .color_format = LV_COLOR_FORMAT_NATIVE,
//         .flags = {
//             .swap_bytes = true,
//         },
//     };
//     lv_disp_t *display = lvgl_port_add_disp(&disp_cfg);
//     lv_obj_t *scr = lv_disp_get_scr_act(display);
//     lv_obj_t *label = lv_label_create(scr);

//     lv_label_set_text_static(label, "Ultrasonic ranger");
//     lv_obj_set_style_text_color(label,
//                                 lv_color_make(0, 0, 255), 0);

//     lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
//     lv_display_flush_ready(display);
//     display_queue = xQueueCreate(10, sizeof(display_event_t));
//     if (display_queue == NULL)
//     {
//         printf("Failed to create display queue\n");
//         return;
//     }
// }

void example_lvgl_demo_ui(lv_disp_t *disp)
{
    lv_obj_t *scr = lv_disp_get_scr_act(disp);
    lv_obj_t *label = lv_label_create(scr);
    // lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR); /* Circular scroll */
    lv_label_set_text(label, "1234567890");
    /* Size of the screen (if you use rotation 90 or 270, please set disp->driver->ver_res) */
#if LVGL_VERSION_MAJOR >= 9
    // lv_obj_set_width(label, lv_display_get_physical_horizontal_resolution(disp));
#else
    lv_obj_set_width(label, disp->driver->hor_res);
#endif
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
}


void setup_display(){
    ESP_LOGI(TAG, "Initialize I2C bus");
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_HOST,
        .sda_io_num = DISPLAY_PIN_NUM_SDA,
        .scl_io_num = DISPLAY_PIN_NUM_SCL,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = DISPLAY_I2C_HW_ADDR,
        .scl_speed_hz = DISPLAY_LCD_PIXEL_CLOCK_HZ,
        .control_phase_bytes = 1,               // According to SSD1306 datasheet
        .lcd_cmd_bits = DISPLAY_LCD_CMD_BITS,   // According to SSD1306 datasheet
        .lcd_param_bits = DISPLAY_LCD_PARAM_BITS, // According to SSD1306 datasheet
#if CONFIG_DISPLAY_LCD_CONTROLLER_SSD1306
        .dc_bit_offset = 6,                     // According to SSD1306 datasheet
#elif CONFIG_DISPLAY_LCD_CONTROLLER_SH1107
        .dc_bit_offset = 0,                     // According to SH1107 datasheet
        .flags =
        {
            .disable_control_phase = 1,
        }
#endif
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = DISPLAY_PIN_NUM_RST,
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5,0,0))
        .color_space = ESP_LCD_COLOR_SPACE_MONOCHROME,
#endif
    };
#if CONFIG_DISPLAY_LCD_CONTROLLER_SSD1306
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,3,0))
    // esp_lcd_panel_ssd1306_config_t ssd1306_config = {
    //     .height = DISPLAY_LCD_V_RES,
    // };
    // panel_config.vendor_config = &ssd1306_config;
#endif
    ESP_LOGI(TAG, "Install SSD1306 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));
#elif CONFIG_DISPLAY_LCD_CONTROLLER_SH1107
    esp_lcd_panel_sh1107_config_t sh1107_config = {
        .contrast = 128,
        .offset = 0x60,
    };
    panel_config.vendor_config = &sh1107_config;
    ESP_LOGI(TAG, "Install SH1107 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh1107(io_handle, &panel_config, &panel_handle));
#endif
// panel_ssd1306_set_gap(panel_handle, ); 
esp_lcd_panel_set_gap(panel_handle, 8, 0);
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

#if CONFIG_DISPLAY_LCD_CONTROLLER_SH1107
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
#endif

    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = DISPLAY_LCD_H_RES * DISPLAY_LCD_V_RES,
        // .double_buffer = true,
        .hres = DISPLAY_LCD_H_RES,
        .vres = DISPLAY_LCD_V_RES,
        .monochrome = true,
#if LVGL_VERSION_MAJOR >= 9
        .color_format = LV_COLOR_FORMAT_I1,
#endif
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = false,
#endif
            .sw_rotate = false,
        }
    };
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
    // Set the offset (gap) in pixels
// lv_display_set_offset(disp, (128-72)/2, 64-40);


ESP_LOGI(TAG, "Display LVGL Scroll Text");
    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (lvgl_port_lock(0)) {
        /* Rotation of the screen */
        lv_disp_set_rotation(disp, LV_DISPLAY_ROTATION_0);

        example_lvgl_demo_ui(disp);
        // Release the mutex
        lvgl_port_unlock();
    }
}

