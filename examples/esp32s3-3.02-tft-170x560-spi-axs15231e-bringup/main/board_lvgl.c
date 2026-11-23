#include "board.h"
#include "board_config.h"
#include "board_lcd.h"

#include <unistd.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_heap_caps.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_check.h"
#include "lvgl.h"
#include "esp_lcd_axs15231b.h"

static const char *TAG = "board_lvgl";

#define LVGL_TICK_PERIOD_MS    2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS (1000 / CONFIG_FREERTOS_HZ)
#define LVGL_TASK_STACK_SIZE   (8 * 1024)
#define LVGL_TASK_PRIORITY     4
#define TOUCH_I2C_NUM              (0)
#define TOUCH_I2C_CLK_HZ           (400000)
#define TOUCH_I2C_ADDR             (0x3B)
#define TOUCH_PROBE_TIMEOUT_MS     (50)

static SemaphoreHandle_t s_lvgl_mux;
static lv_display_t *s_display;
static board_lcd_handles_t s_lcd;

static bool lvgl_notify_flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    (void)disp;
    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;

    lv_draw_sw_rgb565_swap(px_map, (x2 + 1 - x1) * (y2 + 1 - y1));
    board_lcd_draw_bitmap(&s_lcd, x1, y1, x2 + 1, y2 + 1, px_map);
}

static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t touch = lv_indev_get_user_data(indev);
    esp_lcd_touch_point_data_t point;
    uint8_t tp_cnt = 0;

    esp_lcd_touch_read_data(touch);
    esp_err_t err = esp_lcd_touch_get_data(touch, &point, &tp_cnt, 1);

    if (err == ESP_OK && tp_cnt > 0) {
        data->point.x = point.x;
        data->point.y = point.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void lvgl_port_task(void *arg)
{
    uint32_t delay_ms = 0;

    while (1) {
        xSemaphoreTakeRecursive(s_lvgl_mux, portMAX_DELAY);
        delay_ms = lv_timer_handler();
        xSemaphoreGiveRecursive(s_lvgl_mux);

        delay_ms = MAX(delay_ms, LVGL_TASK_MIN_DELAY_MS);
        delay_ms = MIN(delay_ms, LVGL_TASK_MAX_DELAY_MS);
        usleep(1000 * delay_ms);
    }
}

static esp_err_t board_touch_init(lv_display_t *display)
{
#if !BOARD_ENABLE_TOUCH
    ESP_LOGI(TAG, "Touch disabled (BOARD_ENABLE_TOUCH=0)");
    return ESP_OK;
#else

    const i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = TOUCH_I2C_NUM,
        .scl_io_num = BOARD_PIN_TOUCH_SCL,
        .sda_io_num = BOARD_PIN_TOUCH_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus = NULL;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_bus_config, &i2c_bus), TAG, "i2c bus");

    esp_err_t probe = i2c_master_probe(i2c_bus, TOUCH_I2C_ADDR, pdMS_TO_TICKS(TOUCH_PROBE_TIMEOUT_MS));
    if (probe != ESP_OK) {
        ESP_LOGW(TAG, "Touch IC not on I2C (addr 0x%02X), skip touch", TOUCH_I2C_ADDR);
        return ESP_ERR_NOT_FOUND;
    }

    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_AXS15231B_CONFIG();
    tp_io_config.scl_speed_hz = TOUCH_I2C_CLK_HZ;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io), TAG, "touch io");

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BOARD_TOUCH_X_MAX,
        .y_max = BOARD_TOUCH_Y_MAX,
        .rst_gpio_num = BOARD_PIN_TOUCH_RST,
        .int_gpio_num = BOARD_PIN_TOUCH_INT,
        .levels.reset = 0,
        .levels.interrupt = 0,
        .flags.swap_xy = BOARD_TOUCH_SWAP_XY,
        .flags.mirror_x = BOARD_TOUCH_MIRROR_X,
        .flags.mirror_y = BOARD_TOUCH_MIRROR_Y,
    };

    esp_lcd_touch_handle_t touch = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_axs15231b(tp_io, &tp_cfg, &touch), TAG, "touch");

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, display);
    lv_indev_set_user_data(indev, touch);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);

    ESP_LOGI(TAG, "Touch ready %dx%d", BOARD_LCD_H_RES, BOARD_LCD_V_RES);
    return ESP_OK;
#endif
}

static esp_err_t board_lvgl_init(const board_lcd_handles_t *lcd)
{
    lv_init();

    s_display = lv_display_create(BOARD_LCD_H_RES, BOARD_LCD_V_RES);
    size_t draw_buf_sz = BOARD_LCD_H_RES * BOARD_LCD_V_RES * sizeof(lv_color16_t);

    void *buf1 = heap_caps_malloc(draw_buf_sz, MALLOC_CAP_SPIRAM);
    void *buf2 = heap_caps_malloc(draw_buf_sz, MALLOC_CAP_SPIRAM);
    if (!buf1 || !buf2) {
        return ESP_ERR_NO_MEM;
    }

    lv_display_set_buffers(s_display, buf1, buf2, draw_buf_sz, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_user_data(s_display, s_lcd.panel);
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(s_display, lvgl_flush_cb);

    lv_obj_t *scr = lv_display_get_screen_active(s_display);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    const esp_timer_create_args_t tick_args = {
        .callback = &lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_args, &tick_timer), TAG, "tick timer");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000), TAG, "tick start");

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = lvgl_notify_flush_ready,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_register_event_callbacks(lcd->io, &cbs, s_display),
        TAG, "flush cb");

    ESP_RETURN_ON_ERROR(board_touch_init(s_display), TAG, "touch");

    BaseType_t ok = xTaskCreate(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);
    if (ok != pdPASS) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LVGL ready %dx%d", BOARD_LCD_H_RES, BOARD_LCD_V_RES);
    return ESP_OK;
}

esp_err_t board_init(void)
{
    s_lvgl_mux = xSemaphoreCreateRecursiveMutex();
    if (!s_lvgl_mux) {
        return ESP_ERR_NO_MEM;
    }

    board_lcd_handles_t lcd = {0};
    ESP_RETURN_ON_ERROR(board_lcd_init(&lcd), TAG, "lcd");
    s_lcd = lcd;
    ESP_RETURN_ON_ERROR(board_lvgl_init(&lcd), TAG, "lvgl");
    return ESP_OK;
}

lv_display_t *board_get_display(void)
{
    return s_display;
}

bool board_lvgl_lock(uint32_t timeout_ms)
{
    TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_lvgl_mux, ticks) == pdTRUE;
}

void board_lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(s_lvgl_mux);
}
