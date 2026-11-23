#include "board_lcd.h"
#include "board_config.h"

#include <stdlib.h>
#include <sys/param.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_check.h"
#include "lvgl.h"
#include "esp_lcd_axs15231b.h"

static const char *TAG = "board_lcd";

#if BOARD_LCD_USE_QSPI
#define BOARD_LCD_IO_CMD(cmd)    ((((uint32_t)(cmd) & 0xffu) << 8) | (0x02u << 24))
#define BOARD_LCD_IO_COLOR(cmd)  ((((uint32_t)(cmd) & 0xffu) << 8) | (0x32u << 24))
#else
#define BOARD_LCD_IO_CMD(cmd)    (cmd)
#define BOARD_LCD_IO_COLOR(cmd)  (cmd)
#endif

#if LV_COLOR_DEPTH == 16
#define LCD_BPP (16)
#elif LV_COLOR_DEPTH >= 24
#define LCD_BPP (24)
#else
#define LCD_BPP (16)
#endif

#define LCD_SPI_NUM          (SPI2_HOST)

#if BOARD_LCD_HW_ROTATE270
#include "lcd_init_270.inc"
#define LCD_INIT_CMDS       lcd_init_cmds_270
#define LCD_INIT_NAME       "270"
#elif BOARD_LCD_HW_ROTATE90
#include "lcd_init_xy.inc"
#define LCD_INIT_CMDS       lcd_init_cmds_xy
#define LCD_INIT_NAME       "XY"
#else
#include "lcd_init_te.inc"
#define LCD_INIT_CMDS       lcd_init_cmds_te
#define LCD_INIT_NAME       "TE"
#endif

static esp_err_t board_lcd_te_gpio_init(void)
{
    const gpio_config_t te_cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << BOARD_PIN_LCD_TE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    return gpio_config(&te_cfg);
}

static esp_err_t board_lcd_tx_window_color(esp_lcd_panel_io_handle_t io,
                                           int caset0, int caset1_excl,
                                           int raset0, int raset1_excl,
                                           const void *color_data, size_t len)
{
    const int caset1 = caset1_excl - 1;
    const int raset1 = raset1_excl - 1;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, BOARD_LCD_IO_CMD(LCD_CMD_CASET), (uint8_t[]) {
        (caset0 >> 8) & 0xFF, caset0 & 0xFF,
        (caset1 >> 8) & 0xFF, caset1 & 0xFF,
    }, 4), TAG, "caset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, BOARD_LCD_IO_CMD(LCD_CMD_RASET), (uint8_t[]) {
        (raset0 >> 8) & 0xFF, raset0 & 0xFF,
        (raset1 >> 8) & 0xFF, raset1 & 0xFF,
    }, 4), TAG, "raset");
    const int ram_cmd = (raset0 == 0) ? LCD_CMD_RAMWR : LCD_CMD_RAMWRC;
    return esp_lcd_panel_io_tx_color(io, BOARD_LCD_IO_COLOR(ram_cmd), color_data, len);
}

esp_err_t board_lcd_draw_bitmap(const board_lcd_handles_t *lcd,
                                int x_start, int y_start, int x_end, int y_end,
                                const void *color_data)
{
    const int w = x_end - x_start;
    const int h = y_end - y_start;
    ESP_RETURN_ON_FALSE(w > 0 && h > 0, ESP_ERR_INVALID_ARG, TAG, "empty area");
    const size_t len = (size_t)w * (size_t)h * sizeof(uint16_t);
    return board_lcd_tx_window_color(lcd->io, x_start, x_end, y_start, y_end, color_data, len);
}

static esp_err_t board_lcd_fill(const board_lcd_handles_t *lcd, uint16_t color)
{
    const int w = BOARD_LCD_H_RES;
    const int h = BOARD_LCD_V_RES;
    const int rows_per_chunk = 10;
    const size_t chunk_pixels = (size_t)w * rows_per_chunk;
    uint16_t *buf = heap_caps_malloc(chunk_pixels * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "fill buf");
    for (size_t i = 0; i < chunk_pixels; i++) {
        buf[i] = color;
    }

    esp_err_t err = ESP_OK;
    for (int y = 0; y < h && err == ESP_OK; y += rows_per_chunk) {
        const int rows = MIN(rows_per_chunk, h - y);
        const size_t len = (size_t)w * rows * sizeof(uint16_t);
        if (y == 0) {
            err = board_lcd_tx_window_color(lcd->io, 0, w, 0, h, buf, len);
        } else {
            err = esp_lcd_panel_io_tx_color(lcd->io, BOARD_LCD_IO_COLOR(LCD_CMD_RAMWRC), buf, len);
        }
    }
    /* tx_param 会等待之前排队的颜色传输全部完成，之后才能释放 buf */
    if (err == ESP_OK) {
        err = esp_lcd_panel_io_tx_param(lcd->io, BOARD_LCD_IO_CMD(LCD_CMD_NOP), NULL, 0);
    }
    free(buf);
    return err;
}

esp_err_t board_lcd_init(board_lcd_handles_t *out)
{
    const gpio_config_t bl_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BOARD_PIN_LCD_BL,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bl_gpio_config), TAG, "bl gpio");
    ESP_RETURN_ON_ERROR(board_lcd_te_gpio_init(), TAG, "te gpio");

    const size_t max_transfer = BOARD_LCD_H_RES * BOARD_LCD_V_RES * sizeof(uint16_t);

#if BOARD_LCD_USE_QSPI
    const spi_bus_config_t buscfg = AXS15231B_PANEL_BUS_QSPI_CONFIG(
        BOARD_PIN_LCD_SCLK,
        BOARD_PIN_LCD_DATA0, BOARD_PIN_LCD_DATA1,
        BOARD_PIN_LCD_DATA2, BOARD_PIN_LCD_DATA3,
        max_transfer);
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = -1,
        .cs_gpio_num = BOARD_PIN_LCD_CS,
        .pclk_hz = BOARD_LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits = 32,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .flags.quad_mode = true,
    };
#else
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BOARD_PIN_LCD_SCLK,
        .mosi_io_num = BOARD_PIN_LCD_DATA0,
        .miso_io_num = BOARD_PIN_LCD_DATA1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = max_transfer,
    };
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BOARD_PIN_LCD_DC,
        .cs_gpio_num = BOARD_PIN_LCD_CS,
        .pclk_hz = BOARD_LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
#endif

    ESP_RETURN_ON_ERROR(
        spi_bus_initialize((esp_lcd_spi_bus_handle_t)LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO),
        TAG, "spi bus");

    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_NUM, &io_config, &io_handle),
        TAG, "panel io");

    axs15231b_vendor_config_t vendor_config = {
        .init_cmds = LCD_INIT_CMDS,
        .init_cmds_size = sizeof(LCD_INIT_CMDS) / sizeof(axs15231b_lcd_init_cmd_t),
        .flags = {
            .use_qspi_interface = BOARD_LCD_USE_QSPI,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BPP,
        .vendor_config = &vendor_config,
    };

    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_axs15231b(io_handle, &panel_config, &panel_handle), TAG, "panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel_handle), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel_handle), TAG, "init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel_handle, false), TAG, "disp on");

    out->panel = panel_handle;
    out->io = io_handle;

    ESP_RETURN_ON_ERROR(board_lcd_fill(out, 0x0000), TAG, "fill black");
    gpio_set_level(BOARD_PIN_LCD_BL, 1);
    ESP_LOGI(TAG, "LCD ready %dx%d %s %s",
             BOARD_LCD_H_RES, BOARD_LCD_V_RES,
             BOARD_LCD_USE_QSPI ? "QSPI" : "SPI",
             LCD_INIT_NAME);

    return ESP_OK;
}
