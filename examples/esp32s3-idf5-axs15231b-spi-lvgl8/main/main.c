/*
 * SPDX-FileCopyrightText: Copyright 2026 OSPTEK
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * https://github.com/osptek
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"

#include "lvgl.h"
#include "lv_demos.h"

#include "esp_lcd_axs15231b.h"

static const char *TAG = "example";
static SemaphoreHandle_t lvgl_mux = NULL;

/* LCD size */
#define EXAMPLE_LCD_H_RES   (170)
#define EXAMPLE_LCD_V_RES   (560)

#if LV_COLOR_DEPTH == 16
#define LV_COLOR_FORMAT (LV_COLOR_FORMAT_RGB565)
#define LCD_COLOR_DEPTH (16)
#elif LV_COLOR_DEPTH >= 24
#define LV_COLOR_FORMAT (LV_COLOR_FORMAT_RGB888)
#define LCD_COLOR_DEPTH (24)
#endif

/* LCD settings */
#define EXAMPLE_LCD_SPI_NUM         (SPI2_HOST)
#define EXAMPLE_LCD_PIXEL_CLK_HZ    (80 * 1000 * 1000)
#define EXAMPLE_LCD_DRAW_BUFF_HEIGHT (EXAMPLE_LCD_V_RES)

/* LCD pins */
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL 1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_LCD_CS (GPIO_NUM_47)
#define EXAMPLE_PIN_NUM_LCD_DC (GPIO_NUM_14)
#define EXAMPLE_PIN_NUM_LCD_SCLK (GPIO_NUM_15)
#define EXAMPLE_PIN_NUM_LCD_MOSI (GPIO_NUM_13)
#define EXAMPLE_PIN_NUM_LCD_RST (GPIO_NUM_16)
#define EXAMPLE_PIN_NUM_LCD_TE  (GPIO_NUM_9)
#define EXAMPLE_PIN_NUM_BK_LIGHT (GPIO_NUM_38)

/* Touch settings */
#define EXAMPLE_TOUCH_I2C_NUM       (0)
#define EXAMPLE_TOUCH_I2C_CLK_HZ    (400000)

/* LCD touch pins */
#define EXAMPLE_TOUCH_I2C_SCL       (GPIO_NUM_41)
#define EXAMPLE_TOUCH_I2C_SDA       (GPIO_NUM_40)
#define EXAMPLE_TOUCH_RST           (GPIO_NUM_42)
#define EXAMPLE_TOUCH_INT           (GPIO_NUM_39)

esp_lcd_touch_handle_t tp = NULL;

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1
#define EXAMPLE_LVGL_TASK_STACK_SIZE (4 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY 2

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    // 将缓冲区的内容复制到显示的特定区域
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

static SemaphoreHandle_t touch_mux = NULL;

static void example_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *lv_data)
{
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)drv->user_data;
    assert(tp);

    // 创建结构体用于存储触点信息
    esp_lcd_touch_point_data_t point;
    uint8_t tp_cnt = 0;

    /* 1. 读取硬件原始数据 */
    if (xSemaphoreTake(touch_mux, 0) == pdTRUE) {
        esp_lcd_touch_read_data(tp);
        xSemaphoreGive(touch_mux); // 必须释放，防止死锁
    }

    /* 2. 获取解析后的触点数据 */
    // 这里我们只请求 1 个点 (max_point_cnt = 1)
    esp_err_t err = esp_lcd_touch_get_data(tp, &point, &tp_cnt, 1);

    /* 3. 反馈给 LVGL */
    if (err == ESP_OK && tp_cnt > 0) {
        lv_data->point.x = point.x;
        lv_data->point.y = point.y;
        lv_data->state = LV_INDEV_STATE_PRESSED;
        
        // printf("x:%d,y:%d\n", point.x, point.y);
    } else {
        lv_data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void example_touch_isr_cb(esp_lcd_touch_handle_t tp)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(touch_mux, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

static void example_increase_lvgl_tick(void *arg)
{
    /* 告诉 LVGL 已经过去了多少毫秒*/
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static bool example_lvgl_lock(int timeout_ms)
{
    assert(lvgl_mux && "bsp_display_start must be called first");

    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

static void example_lvgl_unlock(void)
{
    assert(lvgl_mux && "bsp_display_start must be called first");
    xSemaphoreGive(lvgl_mux);
}

static void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    while (1)
    {
        // 由于 LVGL API 不是线程安全的，因此锁定互斥体
        if (example_lvgl_lock(-1))
        {
            task_delay_ms = lv_timer_handler();
            // 释放互斥锁
            example_lvgl_unlock();
        }
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS)
        {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        }
        else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS)
        {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

static const axs15231b_lcd_init_cmd_t lcd_init_cmds[] = {
//  {cmd, { data }, data_size, delay_ms}
    {0xBB, (uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0xA5}, 8, 0},
    {0xA0, (uint8_t[]){0x00, 0x10, 0x00, 0x02, 0x00, 0x00, 0x64, 0x3F, 0x20, 0x05, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00}, 17, 0},
    {0xA2, (uint8_t[]){0x30, 0x04, 0x0A, 0x3C, 0xEC, 0x54, 0xC4, 0x30, 0xAC, 0x28, 0x7F, 0x7F, 0x7F, 0x20, 0xF8, 0x10, 0x02, 0xFF, 0xFF, 0xF0, 0x90, 0x01, 0x32, 0xA0, 0x91, 0xC0, 0x20, 0x7F, 0xFF, 0x00, 0x54}, 31, 0},
    {0xD0, (uint8_t[]){0x30, 0xAC, 0x21, 0x24, 0x08, 0x09, 0x10, 0x01, 0xAA, 0x14, 0xC2, 0x00, 0x22, 0x22, 0xAA, 0x03, 0x10, 0x12, 0x40, 0x14, 0x1E, 0x51, 0x15, 0x00, 0x40, 0x10, 0x00, 0x03, 0x3D, 0x12}, 30, 0},
    {0xA3, (uint8_t[]){0xA0, 0x06, 0xAA, 0x08, 0x08, 0x02, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x55, 0x55}, 22, 0},
    {0xC1, (uint8_t[]){0x33, 0x04, 0x02, 0x02, 0x71, 0x05, 0x24, 0x55, 0x02, 0x00, 0x41, 0x00, 0x53, 0xFF, 0xFF, 0xFF, 0x4F, 0x52, 0x00, 0x4F, 0x52, 0x00, 0x45, 0x3B, 0x0B, 0x02, 0x0D, 0x00, 0xFF, 0x40}, 30, 0},
    {0xC3, (uint8_t[]){0x00, 0x00, 0x00, 0x50, 0x03, 0x00, 0x00, 0x00, 0x01, 0x80, 0x01}, 11, 0},
    {0xC4, (uint8_t[]){0x00, 0x24, 0x33, 0x90, 0x50, 0xea, 0x64, 0x32, 0xC8, 0x64, 0xC8, 0x32, 0x90, 0x90, 0x11, 0x06, 0xDC, 0xFA, 0x04, 0x03, 0x80, 0xFE, 0x10, 0x10, 0x00, 0x0A, 0x0A, 0x44, 0x50}, 29, 0},
    {0xC5, (uint8_t[]){0x18, 0x00, 0x00, 0x03, 0xFE, 0x78, 0x33, 0x20, 0x30, 0x10, 0x88, 0xDE, 0x0D, 0x08, 0x0F, 0x0F, 0x01, 0x78, 0x33, 0x20, 0x10, 0x10, 0x80}, 23, 0},
    {0xC6, (uint8_t[]){0x05, 0x0A, 0x05, 0x0A, 0x00, 0xE0, 0x2E, 0x0B, 0x12, 0x22, 0x12, 0x22, 0x01, 0x00, 0x00, 0x3F, 0x6A, 0x18, 0xC8, 0x22}, 20, 0},
    {0xC7, (uint8_t[]){0x50, 0x32, 0x28, 0x00, 0xa2, 0x80, 0x8f, 0x00, 0x80, 0xff, 0x07, 0x11, 0x9F, 0x6f, 0xff, 0x26, 0x0c, 0x0d, 0x0e, 0x0f}, 20, 0},
    {0xC9, (uint8_t[]){0x33, 0x44, 0x44, 0x01}, 4, 0},
    {0xCF, (uint8_t[]){0x34, 0x1E, 0x88, 0x58, 0x13, 0x18, 0x56, 0x18, 0x1E, 0x68, 0xF7, 0x00, 0x65, 0x0C, 0x22, 0xC4, 0x0C, 0x77, 0x22, 0x44, 0xAA, 0x55, 0x04, 0x04, 0x12, 0xA0, 0x08}, 27, 0},
    {0xD5, (uint8_t[]){0x3E, 0x3E, 0x88, 0x00, 0x44, 0x04, 0x78, 0x33, 0x20, 0x78, 0x33, 0x20, 0x04, 0x28, 0xD3, 0x47, 0x03, 0x03, 0x03, 0x03, 0x86, 0x00, 0x00, 0x00, 0x30, 0x52, 0x3f, 0x40, 0x40, 0x96}, 30, 0},
    {0xD6, (uint8_t[]){0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x95, 0x00, 0x01, 0x83, 0x75, 0x36, 0x20, 0x75, 0x36, 0x20, 0x3F, 0x03, 0x03, 0x03, 0x10, 0x10, 0x00, 0x04, 0x51, 0x20, 0x01, 0x00}, 30, 0},
    {0xD7, (uint8_t[]){0x0a, 0x08, 0x0e, 0x0c, 0x1E, 0x18, 0x19, 0x1F, 0x00, 0x1F, 0x1A, 0x1F, 0x3E, 0x3E, 0x04, 0x00, 0x1F, 0x1F, 0x1F}, 19, 0},
    {0xD8, (uint8_t[]){0x0B, 0x09, 0x0F, 0x0D, 0x1E, 0x18, 0x19, 0x1F, 0x01, 0x1F, 0x1A, 0x1F}, 12, 0},
    {0xD9, (uint8_t[]){0x00, 0x0D, 0x0F, 0x09, 0x0B, 0x1F, 0x18, 0x19, 0x1F, 0x01, 0x1E, 0x1A, 0x1F}, 13, 0},
    {0xDD, (uint8_t[]){0x0C, 0x0E, 0x08, 0x0A, 0x1F, 0x18, 0x19, 0x1F, 0x00, 0x1E, 0x1A, 0x1F}, 12, 0},
    {0xDF, (uint8_t[]){0x44, 0x73, 0x4B, 0x69, 0x00, 0x0A, 0x02, 0x90}, 8, 0},
    {0xE0, (uint8_t[]){0x19, 0x20, 0x0A, 0x13, 0x0E, 0x09, 0x12, 0x28, 0xD4, 0x24, 0x0C, 0x35, 0x13, 0x31, 0x36, 0x2f, 0x03}, 17, 0},
    {0xE1, (uint8_t[]){0x38, 0x20, 0x09, 0x12, 0x0E, 0x08, 0x12, 0x28, 0xC5, 0x24, 0x0C, 0x34, 0x12, 0x31, 0x36, 0x2f, 0x27}, 17, 0},
    {0xE2, (uint8_t[]){0x19, 0x20, 0x0A, 0x11, 0x09, 0x06, 0x11, 0x25, 0xD4, 0x22, 0x0B, 0x33, 0x12, 0x2D, 0x32, 0x2f, 0x03}, 17, 0},
    {0xE3, (uint8_t[]){0x38, 0x20, 0x0A, 0x11, 0x09, 0x06, 0x11, 0x25, 0xC4, 0x21, 0x0A, 0x32, 0x11, 0x2C, 0x32, 0x2f, 0x27}, 17, 0},
    {0xE4, (uint8_t[]){0x19, 0x20, 0x0D, 0x14, 0x0D, 0x08, 0x12, 0x2A, 0xD4, 0x26, 0x0E, 0x35, 0x13, 0x34, 0x39, 0x2f, 0x03}, 17, 0},
    {0xE5, (uint8_t[]){0x38, 0x20, 0x0D, 0x13, 0x0D, 0x07, 0x12, 0x29, 0xC4, 0x25, 0x0D, 0x35, 0x12, 0x33, 0x39, 0x2f, 0x27}, 17, 0},
    {0xBB, (uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, 0},
    {0x13, (uint8_t[]){0x00}, 0, 0},
    {0x11, (uint8_t[]){0x00}, 0, 200},
    {0x29, (uint8_t[]){0x00}, 0, 200},
    {0x2C, (uint8_t[]){0x00, 0x00, 0x00, 0x00}, 4, 0},
    {0x22, (uint8_t[]){0x00}, 0, 200},//All Pixels off
};

void app_main(void)
{
    static lv_disp_draw_buf_t disp_buf; // 包含称为绘制缓冲区的内部图形缓冲区
    static lv_disp_drv_t disp_drv;      // 包含回调函数

#if (EXAMPLE_PIN_NUM_BK_LIGHT !=-1)
    /* LCD backlight */
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT};
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
#endif

    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = AXS15231B_PANEL_BUS_SPI_CONFIG(EXAMPLE_PIN_NUM_LCD_SCLK,
                                                                    EXAMPLE_PIN_NUM_LCD_MOSI,
                                                                    EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT * sizeof(uint16_t));

    ESP_ERROR_CHECK(spi_bus_initialize((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = EXAMPLE_PIN_NUM_LCD_DC,
        .cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = example_notify_lvgl_flush_ready,
        .user_ctx = &disp_drv,
    };

    // 将 LCD 连接到 SPI 总线
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_SPI_NUM, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    axs15231b_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,         
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(axs15231b_lcd_init_cmd_t),
        .flags = {
            .use_qspi_interface = 0,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_COLOR_DEPTH,
        .vendor_config = &vendor_config,
    };

    ESP_LOGI(TAG, "Install AXS15231B panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_axs15231b(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    // 在打开屏幕或背光之前，用户可以将预定义的图案刷新到屏幕上
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, false));

    ESP_LOGI(TAG, "Initialize I2C bus");
    const i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = EXAMPLE_TOUCH_I2C_NUM,
        .scl_io_num = EXAMPLE_TOUCH_I2C_SCL,
        .sda_io_num = EXAMPLE_TOUCH_I2C_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus));

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_AXS15231B_CONFIG();
    tp_io_config.scl_speed_hz = EXAMPLE_TOUCH_I2C_CLK_HZ;

    //Attach the TOUCH to the I2C bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io_handle));

    touch_mux = xSemaphoreCreateBinary();
    assert(touch_mux);

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_V_RES,
        .y_max = EXAMPLE_LCD_H_RES,
        .rst_gpio_num = EXAMPLE_TOUCH_RST,
        .int_gpio_num = EXAMPLE_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 1,
            .mirror_x = 1,
            .mirror_y = 0,
        },
        .interrupt_callback = example_touch_isr_cb,
    };

    ESP_LOGI(TAG, "Initialize touch controller AXS15231B");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_axs15231b(tp_io_handle, &tp_cfg, &tp));

#if (EXAMPLE_PIN_NUM_BK_LIGHT !=-1)
    /* LCD backlight on */
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
#endif

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // 分配 LVGL 使用的绘制缓冲区
    // 建议选择绘制缓冲区的大小至少为屏幕大小的 1/10
    lv_color_t *buf1 = heap_caps_malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf2);
    // 初始化 LVGL 绘制缓冲区
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT);

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    disp_drv.full_refresh = 1;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // LVGL 的 Tick 接口（使用 esp_timer 生成 2ms 周期性事件）
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"};
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    static lv_indev_drv_t indev_drv;    // 输入设备驱动程序（触摸）
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev_drv.read_cb = example_lvgl_touch_cb;
    indev_drv.user_data = tp;

    lv_indev_drv_register(&indev_drv);

    lvgl_mux = xSemaphoreCreateMutex();
    assert(lvgl_mux);
    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Display LVGL demos");
    // 由于 LVGL API 不是线程安全的，因此锁定互斥体
    if (example_lvgl_lock(-1))
    {
        lv_demo_widgets(); /* 小部件示例 */
        // lv_demo_music();        /* 类似智能手机的现代音乐播放器演示 */
        // lv_demo_stress();       /* LVGL 压力测试 */
        // lv_demo_benchmark();    /* 用于测量 LVGL 性能或比较不同设置的演示 */

        // 释放互斥锁
        example_lvgl_unlock();
    }
}
