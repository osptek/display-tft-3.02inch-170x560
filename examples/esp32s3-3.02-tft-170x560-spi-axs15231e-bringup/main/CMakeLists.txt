idf_component_register(SRCS "display_test_demo.c" "main.c" "board_lcd.c" "board_lvgl.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_timer lv_osptek_boot_anim)
