#include "display_test_demo.h"

#include <stdint.h>
#include "lvgl.h"

#define COLOR_BG            0x000000
#define COLOR_CARD          0x111214
#define COLOR_CARD_PRESSED  0x1A120B
#define COLOR_LINE          0x26282C
#define COLOR_TEXT          0xF2EEE8
#define COLOR_ACCENT        0xFF7A1A
#define COLOR_MUTED         0xA3A9B1

#define MENU_PAD            14
#define CARD_GAP            10
#define CARD_RADIUS         14
#define CARD_RISE           12
#define CARD_DELAY          250
#define CARD_STAGGER        70
#define CARD_TIME           500
#define SCREEN_FADE_TIME    250
#define PILL_SHOW_TIME      1800
#define PILL_FADE_TIME      400
#define TOUCH_DOT_SIZE      24

typedef void (*demo_build_cb_t)(lv_obj_t *parent);

typedef struct {
    const char *title;
    const char *desc;
    demo_build_cb_t create_page;
    demo_build_cb_t create_preview;
} demo_t;

static void create_color_bars(lv_obj_t *page);
static void create_border_grid(lv_obj_t *page);
static void create_gray_checker(lv_obj_t *page);
static void create_touch_test(lv_obj_t *page);
static void preview_color_bars(lv_obj_t *box);
static void preview_grid(lv_obj_t *box);
static void preview_gray(lv_obj_t *box);
static void preview_touch(lv_obj_t *box);
static void create_white(lv_obj_t *page);
static void preview_white(lv_obj_t *box);

static const demo_t s_demos[] = {
    {"Color Bars", "8-color bars",    create_color_bars,   preview_color_bars},
    {"Grid",       "Border & grid",   create_border_grid,  preview_grid},
    {"Grayscale",  "Gray & checker",  create_gray_checker, preview_gray},
    {"White",      "Full white",      create_white,        preview_white},
    {"Touch",      "Coordinate test", create_touch_test,   preview_touch},
};

#define DEMO_COUNT ((int)(sizeof(s_demos) / sizeof(s_demos[0])))

static lv_obj_t *s_menu;

static lv_obj_t *create_plain(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *create_fill(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *obj = create_plain(parent);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                              uint32_t color, int32_t letter_space)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_letter_space(label, letter_space, 0);
    return label;
}

static void prepare_screen(lv_obj_t *screen)
{
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

static void anim_opa_cb(void *obj, int32_t v)
{
    lv_obj_set_style_opa(obj, (lv_opa_t)v, 0);
}

static void anim_translate_y_cb(void *obj, int32_t v)
{
    lv_obj_set_style_translate_y(obj, v, 0);
}

static lv_anim_t *anim_start(lv_obj_t *obj, lv_anim_exec_xcb_t exec_cb, int32_t from, int32_t to,
                             uint32_t delay, uint32_t time)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, exec_cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_duration(&a, time);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    return lv_anim_start(&a);
}

/* ===== 全屏测试页 ===== */

static lv_obj_t *create_grow_block(lv_obj_t *parent, lv_color_t color, bool horizontal)
{
    lv_obj_t *block = create_fill(parent, color);
    if (horizontal) {
        lv_obj_set_size(block, 0, LV_PCT(100));
    } else {
        lv_obj_set_size(block, LV_PCT(100), 0);
    }
    lv_obj_set_flex_grow(block, 1);
    return block;
}

static lv_obj_t *create_flex_box(lv_obj_t *parent, lv_flex_flow_t flow)
{
    lv_obj_t *box = create_plain(parent);
    lv_obj_set_layout(box, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(box, flow);
    return box;
}

static void create_color_bars(lv_obj_t *page)
{
    static const uint32_t colors[] = {
        0xFFFFFF, 0xFFFF00, 0x00FFFF, 0x00FF00,
        0xFF00FF, 0xFF0000, 0x0000FF, 0x000000,
    };

    lv_obj_set_layout(page, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_ROW);
    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        create_grow_block(page, lv_color_hex(colors[i]), true);
    }
}

static void create_border_grid(lv_obj_t *page)
{
    lv_obj_set_style_border_color(page, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(page, 3, 0);
    lv_obj_set_style_border_opa(page, LV_OPA_COVER, 0);

    for (int i = 1; i < 4; i++) {
        lv_obj_t *vertical = create_fill(page, lv_color_hex(0x404040));
        lv_obj_set_size(vertical, 1, LV_PCT(100));
        lv_obj_align(vertical, LV_ALIGN_LEFT_MID, LV_PCT(i * 25), 0);

        lv_obj_t *horizontal = create_fill(page, lv_color_hex(0x404040));
        lv_obj_set_size(horizontal, LV_PCT(100), 1);
        lv_obj_align(horizontal, LV_ALIGN_TOP_MID, 0, LV_PCT(i * 25));
    }

    lv_obj_t *vertical_center = create_fill(page, lv_color_hex(0x00FF00));
    lv_obj_set_size(vertical_center, 2, LV_PCT(100));
    lv_obj_center(vertical_center);

    lv_obj_t *horizontal_center = create_fill(page, lv_color_hex(0x00FF00));
    lv_obj_set_size(horizontal_center, LV_PCT(100), 2);
    lv_obj_center(horizontal_center);

    static const uint32_t corner_colors[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF};
    static const lv_align_t corner_align[] = {
        LV_ALIGN_TOP_LEFT, LV_ALIGN_TOP_RIGHT, LV_ALIGN_BOTTOM_LEFT, LV_ALIGN_BOTTOM_RIGHT,
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *corner = create_fill(page, lv_color_hex(corner_colors[i]));
        lv_obj_set_size(corner, 14, 14);
        int x_offset = (i == 1 || i == 3) ? -6 : 6;
        int y_offset = (i >= 2) ? -6 : 6;
        lv_obj_align(corner, corner_align[i], x_offset, y_offset);
    }
}

static void create_gray_checker(lv_obj_t *page)
{
    lv_obj_set_layout(page, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *gray_row = create_flex_box(page, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(gray_row, LV_PCT(100), 0);
    lv_obj_set_flex_grow(gray_row, 1);
    for (int i = 0; i < 8; i++) {
        uint8_t level = (uint8_t)(i * 255 / 7);
        create_grow_block(gray_row, lv_color_make(level, level, level), true);
    }

    lv_obj_t *checker = create_flex_box(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(checker, LV_PCT(100), 0);
    lv_obj_set_flex_grow(checker, 1);
    for (int row = 0; row < 4; row++) {
        lv_obj_t *checker_row = create_flex_box(checker, LV_FLEX_FLOW_ROW);
        lv_obj_set_size(checker_row, LV_PCT(100), 0);
        lv_obj_set_flex_grow(checker_row, 1);
        for (int col = 0; col < 12; col++) {
            uint32_t color = ((row + col) & 1) ? 0xFFFFFF : 0x000000;
            create_grow_block(checker_row, lv_color_hex(color), true);
        }
    }
}

static void touch_page_event_cb(lv_event_t *e)
{
    lv_obj_t *page = lv_event_get_current_target(e);
    lv_obj_t *dot = lv_obj_get_user_data(page);
    lv_obj_t *coord = lv_obj_get_user_data(dot);
    lv_obj_t *hint = lv_obj_get_user_data(coord);
    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }

    lv_point_t point;
    lv_area_t area;
    lv_indev_get_point(indev, &point);
    lv_obj_get_coords(page, &area);
    int32_t x = point.x - area.x1;
    int32_t y = point.y - area.y1;

    lv_obj_set_pos(dot, x - TOUCH_DOT_SIZE / 2, y - TOUCH_DOT_SIZE / 2);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(hint, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_fmt(coord, "X %d   Y %d", (int)x, (int)y);
}

static void create_touch_test(lv_obj_t *page)
{
    for (int i = 1; i < 4; i++) {
        lv_obj_t *vertical = create_fill(page, lv_color_hex(0x1C1D20));
        lv_obj_set_size(vertical, 1, LV_PCT(100));
        lv_obj_align(vertical, LV_ALIGN_LEFT_MID, LV_PCT(i * 25), 0);

        lv_obj_t *horizontal = create_fill(page, lv_color_hex(0x1C1D20));
        lv_obj_set_size(horizontal, LV_PCT(100), 1);
        lv_obj_align(horizontal, LV_ALIGN_TOP_MID, 0, LV_PCT(i * 25));
    }

    lv_obj_t *hint = create_label(page, "TAP ANYWHERE", &lv_font_montserrat_12, COLOR_MUTED, 3);
    lv_obj_center(hint);

    lv_obj_t *coord = create_label(page, "", &lv_font_montserrat_14, COLOR_TEXT, 1);
    lv_obj_align(coord, LV_ALIGN_BOTTOM_RIGHT, -12, -10);

    lv_obj_t *dot = create_plain(page);
    lv_obj_set_size(dot, TOUCH_DOT_SIZE, TOUCH_DOT_SIZE);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_30, 0);
    lv_obj_set_style_border_color(dot, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_border_width(dot, 2, 0);
    lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_user_data(page, dot);
    lv_obj_set_user_data(dot, coord);
    lv_obj_set_user_data(coord, hint);
    lv_obj_add_event_cb(page, touch_page_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(page, touch_page_event_cb, LV_EVENT_PRESSING, NULL);
}

/* ===== 测试页导航 ===== */

static void pill_hidden_cb(lv_anim_t *a)
{
    lv_obj_add_flag(a->var, LV_OBJ_FLAG_HIDDEN);
}

static void pill_show(lv_obj_t *pill)
{
    lv_anim_delete(pill, NULL);
    lv_obj_remove_flag(pill, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(pill, LV_OPA_COVER, 0);

    lv_anim_t *a = anim_start(pill, anim_opa_cb, LV_OPA_COVER, LV_OPA_TRANSP,
                              PILL_SHOW_TIME, PILL_FADE_TIME);
    lv_anim_set_completed_cb(a, pill_hidden_cb);
}

static void page_clicked_cb(lv_event_t *e)
{
    lv_obj_t *page = lv_event_get_current_target(e);
    pill_show(lv_obj_get_user_data(lv_obj_get_screen(page)));
}

static void menu_open_cb(lv_event_t *e)
{
    (void)e;
    lv_screen_load_anim(s_menu, LV_SCREEN_LOAD_ANIM_FADE_IN, SCREEN_FADE_TIME, 0, true);
}

static lv_obj_t *create_menu_pill(lv_obj_t *screen)
{
    lv_obj_t *pill = lv_obj_create(screen);
    lv_obj_remove_style_all(pill);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(pill, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_70, 0);
    lv_obj_set_style_border_color(pill, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_border_width(pill, 1, 0);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor(pill, 12, 0);
    lv_obj_set_style_pad_ver(pill, 6, 0);
    lv_obj_set_ext_click_area(pill, 10);
    lv_obj_align(pill, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(pill, menu_open_cb, LV_EVENT_CLICKED, NULL);

    create_label(pill, LV_SYMBOL_LEFT "  MENU", &lv_font_montserrat_12, COLOR_TEXT, 2);
    return pill;
}

static void demo_open(int index)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    prepare_screen(screen);

    lv_obj_t *tileview = lv_tileview_create(screen);
    lv_obj_set_size(tileview, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(tileview, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(tileview, 0, 0);
    lv_obj_set_style_pad_all(tileview, 0, 0);
    lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < DEMO_COUNT; i++) {
        lv_dir_t dir = LV_DIR_NONE;
        if (i > 0) {
            dir |= LV_DIR_LEFT;
        }
        if (i < DEMO_COUNT - 1) {
            dir |= LV_DIR_RIGHT;
        }
        lv_obj_t *page = lv_tileview_add_tile(tileview, (uint8_t)i, 0, dir);
        lv_obj_set_style_bg_color(page, lv_color_hex(COLOR_BG), 0);
        lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(page, 0, 0);
        lv_obj_set_style_pad_gap(page, 0, 0);
        lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);
        s_demos[i].create_page(page);
        lv_obj_add_event_cb(page, page_clicked_cb, LV_EVENT_CLICKED, NULL);
    }
    lv_tileview_set_tile_by_index(tileview, (uint32_t)index, 0, LV_ANIM_OFF);

    lv_obj_t *pill = create_menu_pill(screen);
    lv_obj_set_user_data(screen, pill);
    pill_show(pill);

    lv_screen_load_anim(screen, LV_SCREEN_LOAD_ANIM_FADE_IN, SCREEN_FADE_TIME, 0, false);
}

/* ===== 菜单卡片预览 ===== */

static void preview_color_bars(lv_obj_t *box)
{
    static const uint32_t colors[] = {
        0xFFFFFF, 0xFFFF00, 0x00FFFF, 0x00FF00,
        0xFF00FF, 0xFF0000, 0x0000FF, 0x000000,
    };

    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_clip_corner(box, true, 0);
    lv_obj_set_layout(box, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        create_grow_block(box, lv_color_hex(colors[i]), true);
    }
}

static void preview_grid(lv_obj_t *box)
{
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_border_width(box, 1, 0);

    lv_obj_t *vertical = create_fill(box, lv_color_hex(COLOR_ACCENT));
    lv_obj_set_size(vertical, 1, LV_PCT(100));
    lv_obj_center(vertical);

    lv_obj_t *horizontal = create_fill(box, lv_color_hex(COLOR_ACCENT));
    lv_obj_set_size(horizontal, LV_PCT(100), 1);
    lv_obj_center(horizontal);
}

static void preview_gray(lv_obj_t *box)
{
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_grad_color(box, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_grad_dir(box, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_border_width(box, 1, 0);
}

static void create_white(lv_obj_t *page)
{
    lv_obj_set_style_bg_color(page, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
}

static void preview_white(lv_obj_t *box)
{
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
}

static void preview_touch(lv_obj_t *box)
{
    lv_obj_t *line = create_fill(box, lv_color_hex(COLOR_LINE));
    lv_obj_set_size(line, LV_PCT(100), 1);
    lv_obj_center(line);

    lv_obj_t *ring = create_plain(box);
    lv_obj_set_size(ring, 14, 14);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ring, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_center(ring);
}

/* ===== 菜单 ===== */

static void card_clicked_cb(lv_event_t *e)
{
    demo_open((int)(intptr_t)lv_event_get_user_data(e));
}

static void create_card(lv_obj_t *row, int index, bool landscape)
{
    const demo_t *demo = &s_demos[index];

    lv_obj_t *card = lv_obj_create(row);
    lv_obj_remove_style_all(card);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    if (landscape) {
        lv_obj_set_size(card, 0, LV_PCT(100));
    } else {
        lv_obj_set_size(card, LV_PCT(100), 0);
    }
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_CARD), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, CARD_RADIUS, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_CARD_PRESSED), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_ACCENT), LV_STATE_PRESSED);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 3, 0);

    char number[4];
    lv_snprintf(number, sizeof(number), "%02d", index + 1);
    create_label(card, number, &lv_font_montserrat_12, COLOR_ACCENT, 2);
    create_label(card, demo->title, &lv_font_montserrat_16, COLOR_TEXT, 0);
    create_label(card, demo->desc, &lv_font_montserrat_12, COLOR_MUTED, 0);

    lv_obj_t *spacer = create_plain(card);
    lv_obj_set_size(spacer, LV_PCT(100), 0);
    lv_obj_set_flex_grow(spacer, 1);

    lv_obj_t *preview = create_plain(card);
    lv_obj_set_size(preview, LV_PCT(100), 16);
    demo->create_preview(preview);

    lv_obj_add_event_cb(card, card_clicked_cb, LV_EVENT_CLICKED, (void *)(intptr_t)index);

    uint32_t delay = CARD_DELAY + (uint32_t)index * CARD_STAGGER;
    lv_obj_set_style_opa(card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_translate_y(card, CARD_RISE, 0);
    anim_start(card, anim_opa_cb, LV_OPA_TRANSP, LV_OPA_COVER, delay, CARD_TIME);
    anim_start(card, anim_translate_y_cb, CARD_RISE, 0, delay, CARD_TIME);
}

static void create_header(lv_obj_t *screen)
{
    lv_obj_t *header = create_flex_box(screen, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *brand = create_flex_box(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(brand, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(brand, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(brand, 8, 0);

    lv_obj_t *dot = create_fill(brand, lv_color_hex(COLOR_ACCENT));
    lv_obj_set_size(dot, 6, 6);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    create_label(brand, "DISPLAY LAB", &lv_font_montserrat_12, COLOR_TEXT, 3);

    lv_obj_t *info = create_label(header, "", &lv_font_montserrat_12, COLOR_MUTED, 1);
    lv_label_set_text_fmt(info, "AXS15231E  %dx%d",
                          (int)lv_display_get_horizontal_resolution(NULL),
                          (int)lv_display_get_vertical_resolution(NULL));
}

void display_test_demo_create(lv_obj_t *screen)
{
    s_menu = screen;
    prepare_screen(screen);

    bool landscape = lv_display_get_horizontal_resolution(NULL) >=
                     lv_display_get_vertical_resolution(NULL);

    lv_obj_set_layout(screen, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, MENU_PAD, 0);
    lv_obj_set_style_pad_row(screen, CARD_GAP, 0);

    create_header(screen);

    lv_obj_t *row = create_flex_box(screen, landscape ? LV_FLEX_FLOW_ROW : LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(row, LV_PCT(100), 0);
    lv_obj_set_flex_grow(row, 1);
    lv_obj_set_style_pad_gap(row, CARD_GAP, 0);

    for (int i = 0; i < DEMO_COUNT; i++) {
        create_card(row, i, landscape);
    }
}
