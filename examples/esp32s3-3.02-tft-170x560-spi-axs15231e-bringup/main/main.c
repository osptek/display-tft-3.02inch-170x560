#include "esp_err.h"
#include "esp_log.h"
#include "board.h"
#include "lvgl.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_ERROR_CHECK(board_init());

    if (board_lvgl_lock(UINT32_MAX)) {
        lv_obj_t *scr = lv_screen_active();
        lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
        board_lvgl_unlock();
    } else {
        ESP_LOGE(TAG, "LVGL lock failed");
    }
}
