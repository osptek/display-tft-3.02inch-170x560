#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

typedef struct {
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t io;
} board_lcd_handles_t;

esp_err_t board_lcd_init(board_lcd_handles_t *out);

/* Host-addressed window write (170x560 TE or 560x170 XY, per board_config.h). */
esp_err_t board_lcd_draw_bitmap(const board_lcd_handles_t *lcd,
                                int x_start, int y_start, int x_end, int y_end,
                                const void *color_data);
