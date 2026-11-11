/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
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
#include "esp_lcd_panel_interface.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_rom_gpio.h"
#include "esp_lcd_axs15231b.h"
#include "bsp_err_check.h"

#include "lv_port.h"
#include "display.h"
#include "esp_bsp.h"

static const char *TAG = "example";

static const axs15231b_lcd_init_cmd_t lcd_init_cmds[] = {
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
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x29, (uint8_t[]){0x00}, 0, 200},
    {0x2C, (uint8_t[]){0x00, 0x00, 0x00, 0x00}, 4, 0},
    {0x22, (uint8_t[]){0x00}, 0, 200},//All Pixels off
};

typedef struct {
    SemaphoreHandle_t te_v_sync_sem;    /*!< Semaphore for vertical synchronization */
    SemaphoreHandle_t te_catch_sem;     /*!< Semaphore for tear catch */
    uint32_t time_Tvdl;                 /*!< tvdl = The display panel is updated from the Frame Memory */
    uint32_t time_Tvdh;                 /*!< tvdh = The display panel is not updated from the Frame Memory */
    uint32_t te_timestamp;              /*!< Tear record timestamp */
    portMUX_TYPE lock;                  /*!< Lock for read/write */
} bsp_lcd_tear_t;

typedef struct {
    SemaphoreHandle_t tp_intr_event;    /*!< Semaphore for tp interrupt */
    lv_disp_rot_t rotate;               /*!< Rotation configuration for the display */
} bsp_touch_int_t;

static lv_disp_t *disp;
static lv_indev_t *disp_indev = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;
static esp_lcd_touch_handle_t tp = NULL;   // LCD touch handle
static esp_lcd_panel_handle_t panel_handle = NULL;

static bool i2c_initialized = false;

esp_err_t bsp_i2c_init(void)
{
    /* I2C was initialized before */
    if (i2c_initialized) {
        return ESP_OK;
    }

    const i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = BSP_I2C_NUM,
        .sda_io_num = EXAMPLE_PIN_NUM_I2C_TOUCH_SDA,
        .scl_io_num = EXAMPLE_PIN_NUM_I2C_TOUCH_SCL,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&i2c_bus_config, &i2c_bus));

    i2c_initialized = true;

    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    BSP_ERROR_CHECK_RETURN_ERR(i2c_del_master_bus(i2c_bus));
    i2c_initialized = false;
    return ESP_OK;
}

// 用于表示命令和参数的位编号
#define LCD_LEDC_CH            1

static esp_err_t bsp_display_brightness_init(void)
{
    // 设置用于PWM背光控制的LEDC外设
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = EXAMPLE_PIN_NUM_SPI_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = 1,
        .duty = 0,
        .hpoint = 0
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&LCD_backlight_timer));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_channel_config(&LCD_backlight_channel));

    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // LEDC resolution set to 10bits, thus: 100% = 1023
    BSP_ERROR_CHECK_RETURN_ERR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));

    return ESP_OK;
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

static bool bsp_display_sync_cb(void *arg)
{
    assert(arg);
    bsp_lcd_tear_t *tear_handle = (bsp_lcd_tear_t *)arg;

    if (tear_handle->te_catch_sem) {
        xSemaphoreGive(tear_handle->te_catch_sem);
    }

    if (tear_handle->te_v_sync_sem) {

        xSemaphoreTake(tear_handle->te_v_sync_sem, portMAX_DELAY);
    }
    return true;
}

static void bsp_display_sync_task(void *arg)
{
    assert(arg);
    bsp_lcd_tear_t *tear_handle = (bsp_lcd_tear_t *)arg;

    while (true) {
        if (pdPASS != xSemaphoreTake(tear_handle->te_catch_sem, pdMS_TO_TICKS(tear_handle->time_Tvdl))) {
            xSemaphoreTake(tear_handle->te_v_sync_sem, 0);
        }
    }
    vTaskDelete(NULL);
}

static void bsp_display_tear_interrupt(void *arg)
{
    assert(arg);
    bsp_lcd_tear_t *tear_handle = (bsp_lcd_tear_t *)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (tear_handle->te_v_sync_sem) {
        portENTER_CRITICAL_ISR(&tear_handle->lock);
        tear_handle->te_timestamp = esp_log_timestamp();
        portEXIT_CRITICAL_ISR(&tear_handle->lock);
        xSemaphoreGiveFromISR(tear_handle->te_v_sync_sem, &xHigherPriorityTaskWoken);

        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    esp_err_t ret = ESP_OK;
    assert(config != NULL && config->max_transfer_sz > 0);

    SemaphoreHandle_t te_catch_sem = NULL;
    SemaphoreHandle_t te_v_sync_sem = NULL;
    bsp_lcd_tear_t *tear_ctx = NULL;

    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = AXS15231B_PANEL_BUS_SPI_CONFIG(
                                        EXAMPLE_PIN_NUM_SPI_PCLK,
                                        EXAMPLE_PIN_NUM_SPI_MOSI,
                                        config->max_transfer_sz);
    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {                                     
        .cs_gpio_num = EXAMPLE_PIN_NUM_SPI_CS,                                      
        .dc_gpio_num = EXAMPLE_PIN_NUM_SPI_DC,                                      
        .spi_mode = 0,                                          
        .pclk_hz = EXAMPLE_LCD_SPI_CLK_SPEED_HZ,                           
        .trans_queue_depth = 10,                                
        .on_color_trans_done = NULL,                              
        .user_ctx = NULL,                                     
        .lcd_cmd_bits = 8,                                     
        .lcd_param_bits = 8,                                                                                     
    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_SPI_HOST, &io_config, ret_io));

    ESP_LOGI(TAG, "Install LCD driver of axs15231b");
    const axs15231b_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_SPI_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
        .vendor_config = (void *) &vendor_config,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_axs15231b(*ret_io, &panel_config, ret_panel));

    esp_lcd_panel_reset(*ret_panel);
    esp_lcd_panel_init(*ret_panel);
    esp_lcd_panel_disp_on_off(*ret_panel, false);

    if (config->tear_cfg.te_gpio_num > 0) {

        tear_ctx = malloc(sizeof(bsp_lcd_tear_t));
        ESP_GOTO_ON_FALSE(tear_ctx, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for tear_ctx allocation!");

        te_v_sync_sem = xSemaphoreCreateCounting(1, 0);
        ESP_GOTO_ON_FALSE(te_v_sync_sem, ESP_ERR_NO_MEM, err, TAG, "Failed to create te_v_sync_sem Semaphore");
        tear_ctx->te_v_sync_sem = te_v_sync_sem;

        te_catch_sem = xSemaphoreCreateCounting(1, 0);
        ESP_GOTO_ON_FALSE(te_catch_sem, ESP_ERR_NO_MEM, err, TAG, "Failed to create te_catch_sem Semaphore");
        tear_ctx->te_catch_sem = te_catch_sem;

        tear_ctx->time_Tvdl = config->tear_cfg.time_Tvdl;
        tear_ctx->time_Tvdh = config->tear_cfg.time_Tvdh;

        tear_ctx->lock.owner = portMUX_FREE_VAL;
        tear_ctx->lock.count = 0;

        const gpio_config_t te_detect_cfg = {
            .intr_type = config->tear_cfg.tear_intr_type,
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = BIT64(config->tear_cfg.te_gpio_num),
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_ENABLE,
        };

        ESP_ERROR_CHECK(gpio_config(&te_detect_cfg));
        gpio_install_isr_service(0);
        ESP_ERROR_CHECK(gpio_isr_handler_add(config->tear_cfg.te_gpio_num, bsp_display_tear_interrupt, tear_ctx));

        BaseType_t res;
        if (config->tear_cfg.task_affinity < 0) {
            res = xTaskCreate(bsp_display_sync_task, "Tear task", config->tear_cfg.task_stack, tear_ctx, config->tear_cfg.task_priority, NULL);
        } else {
            res = xTaskCreatePinnedToCore(bsp_display_sync_task, "Tear task", config->tear_cfg.task_stack, tear_ctx, config->tear_cfg.task_priority, NULL, config->tear_cfg.task_affinity);
        }
        ESP_GOTO_ON_FALSE(res == pdPASS, ESP_FAIL, err, TAG, "Create Sync task fail!");
    }

    (*ret_panel)->user_data = (void *)tear_ctx;

    return ret;

err:
    if (te_v_sync_sem) {
        vSemaphoreDelete(te_v_sync_sem);
    }
    if (te_catch_sem) {
        vSemaphoreDelete(te_catch_sem);
    }
    if (tear_ctx) {
        free(tear_ctx);
    }
    if (*ret_panel) {
        esp_lcd_panel_del(*ret_panel);
    }
    if (*ret_io) {
        esp_lcd_panel_io_del(*ret_io);
    }
    spi_bus_free(EXAMPLE_LCD_SPI_HOST);
    return ret;
}

static lv_disp_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);
    esp_lcd_panel_io_handle_t io_handle = NULL;

    uint32_t hres;
    uint32_t vres;

    /**
    * If the transmission time exceeds the refresh period (time_Tvdl), adopt a 2x period,
    * and start data transmission at the falling edge.
    */
    hres = EXAMPLE_LCD_SPI_H_RES;
    vres = EXAMPLE_LCD_SPI_V_RES;
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = hres * vres * sizeof(uint16_t),
        .tear_cfg = BSP_SYNC_TASK_CONFIG(EXAMPLE_PIN_NUM_SPI_TE, GPIO_INTR_NEGEDGE),
    };
    bsp_display_new(&bsp_disp_cfg, &panel_handle, &io_handle);

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = cfg->buffer_size,
        .sw_rotate = cfg->rotate,
        .hres = hres,
        .vres = vres,
        .trans_size = hres * vres / 10,
        .draw_wait_cb = bsp_display_sync_cb,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
        },
    };

    if (disp_cfg.sw_rotate == LV_DISP_ROT_180 || disp_cfg.sw_rotate == LV_DISP_ROT_NONE) {
        disp_cfg.hres = hres;
        disp_cfg.vres = vres;
    } else {
        disp_cfg.hres = vres;
        disp_cfg.vres = hres;
    }

    return lvgl_port_add_disp(&disp_cfg);
}

static bool bsp_touch_sync_cb(void *arg)
{
    assert(arg);
    bool touch_interrupt = false;
    bsp_touch_int_t *touch_handle = (bsp_touch_int_t *)arg;

    if (touch_handle && touch_handle->tp_intr_event) {
        if (xSemaphoreTake(touch_handle->tp_intr_event, 0) == pdTRUE) {
            touch_interrupt = true;
        }
    } else {
        touch_interrupt = true;
    }

    return touch_interrupt;
}

static void bsp_touch_interrupt_cb(esp_lcd_touch_handle_t tp)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    bsp_touch_int_t *touch_handle = (bsp_touch_int_t *)tp->config.user_data;

    xSemaphoreGiveFromISR(touch_handle->tp_intr_event, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

static void bsp_touch_process_points_cb(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    uint16_t tmp;
    bsp_touch_int_t *touch_handle = (bsp_touch_int_t *)tp->config.user_data;

    for (int i = 0; i < *point_num; i++) {
        if (LV_DISP_ROT_270 == touch_handle->rotate) {
            tmp = x[i];
            x[i] = tp->config.y_max - y[i];
            y[i] = tmp;
        } else if (LV_DISP_ROT_180 == touch_handle->rotate) {
            tmp = x[i];
            x[i] = tp->config.x_max - x[i];
            y[i] = tp->config.y_max - y[i];
        } else if (LV_DISP_ROT_90 == touch_handle->rotate) {
            // tmp = x[i];
            // x[i] = y[i];
            // y[i] = tp->config.x_max - tmp;
            tmp = x[i];
            x[i] = tp->config.x_max - x[i];
            y[i] = tp->config.y_max - y[i];
        }
    }
}

esp_err_t bsp_touch_new(const bsp_display_cfg_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    esp_err_t ret = ESP_OK;

    /* Initialize I2C */
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());

    SemaphoreHandle_t tp_intr_event = NULL;
    bsp_touch_int_t *touch_ctx = NULL;

    /* Initialize touch */
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_SPI_V_RES,
        .y_max = EXAMPLE_LCD_SPI_H_RES,
        .rst_gpio_num = EXAMPLE_PIN_NUM_I2C_TOUCH_RST, // Shared with LCD reset
        .int_gpio_num = EXAMPLE_PIN_NUM_I2C_TOUCH_INT,
        .process_coordinates = bsp_touch_process_points_cb,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_AXS15231B_CONFIG();
    tp_io_config.scl_speed_hz = BSP_I2C_CLK_SPEED_HZ;

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io_handle), TAG, "");
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_axs15231b(tp_io_handle, &tp_cfg, &tp_handle), TAG, "New cst816 failed");

    touch_ctx = malloc(sizeof(bsp_touch_int_t));
    ESP_GOTO_ON_FALSE(touch_ctx, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for touch_ctx allocation!");

    if (tp_cfg.int_gpio_num > 0) {

        tp_intr_event = xSemaphoreCreateBinary();
        ESP_GOTO_ON_FALSE(tp_intr_event, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for tp_intr_event allocation!");
        touch_ctx->tp_intr_event = tp_intr_event;
        esp_lcd_touch_register_interrupt_callback_with_data(tp_handle, bsp_touch_interrupt_cb, (void *)touch_ctx);
    } else {
        touch_ctx->tp_intr_event = NULL;
    }
    touch_ctx->rotate = config->rotate;
    tp_handle->config.user_data = touch_ctx;

    *ret_touch = tp_handle;

    return ESP_OK;
err:
    if (tp_intr_event) {
        vSemaphoreDelete(tp_intr_event);
    }
    if (touch_ctx) {
        free(touch_ctx);
    }
    if (tp_handle) {
        esp_lcd_touch_del(tp_handle);
    }
    if (tp_io_handle) {
        esp_lcd_panel_io_del(tp_io_handle);
    }
    return ret;
}

static lv_indev_t *bsp_display_indev_init(const bsp_display_cfg_t *config, lv_disp_t *disp)
{
    BSP_ERROR_CHECK_RETURN_NULL(bsp_touch_new(config, &tp));
    assert(tp);

    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = tp,
        .touch_wait_cb = bsp_touch_sync_cb,
    };

    return lvgl_port_add_touch(&touch_cfg);
}

lv_disp_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    BSP_ERROR_CHECK_RETURN_NULL(lvgl_port_init(&cfg->lvgl_port_cfg));

    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_brightness_init());

    BSP_NULL_CHECK(disp = bsp_display_lcd_init(cfg), NULL);

    BSP_NULL_CHECK(disp_indev = bsp_display_indev_init(cfg, disp), NULL);

    return disp;
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return disp_indev;
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    lvgl_port_unlock();
}
