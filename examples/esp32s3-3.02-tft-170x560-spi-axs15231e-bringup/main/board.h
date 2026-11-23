#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_init(void);
lv_display_t *board_get_display(void);
bool board_lvgl_lock(uint32_t timeout_ms);
void board_lvgl_unlock(void);

#ifdef __cplusplus
}
#endif
