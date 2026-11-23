/*
 * SPDX-FileCopyrightText: Copyright 2026 OSPTEK
 * SPDX-License-Identifier: CC-BY-4.0
 */

/**
 * OSPTEK boot animation, ported from apps/reference/1 · 开机画面-html/Main.dc.html
 *
 * "ope" rises in, "n" rises then shrinks away while "s", "t", "k" grow
 * in between, then "®" and "TOUCH TO BEGIN" fade in and the tag breathes.
 *
 * All sizes come from the 480x480 design and are scaled by
 * min(width, height) / 480 of the parent, so any resolution works.
 */

#include "lv_osptek_boot_anim.h"

#if !LV_USE_TINY_TTF || !LV_USE_FLEX
#error "lv_osptek_boot_anim needs LV_USE_TINY_TTF and LV_USE_FLEX (menuconfig: LVGL)"
#endif

/*********************
 *      DEFINES
 *********************/

#define DESIGN_SIZE         480

#define COLOR_BG            0x000000    /*Also the background of the next screen*/
#define COLOR_TEXT          0xF2EEE8
#define COLOR_ACCENT        0xFF7A1A
#define COLOR_MARK          0xA3A9B1
#define COLOR_TAG           0xC9CDD3

#define LOGO_FONT_SIZE      104
#define MARK_FONT_SIZE      18
#define TAG_FONT_SIZE       11
#define TAG_FONT_SIZE_MIN   8     /*Keep the tag readable on small screens*/
#define COLUMN_GAP          30
#define MARK_MARGIN_TOP     12
#define MARK_MARGIN_LEFT    4
#define RISE_OFFSET         20
#define GROW_OFFSET         16

/*Values in 1/1000 em*/
#define LINE_HEIGHT_EM      1100
#define LOGO_SPACING_EM     (-40)
#define TAG_SPACING_EM      380
#define LETTER_MAX_W_EM     700

#define RISE_TIME           800
#define GROW_TIME           800
#define SHRINK_TIME         700
#define SHRINK_DELAY        1750
#define OUT_RISE_DELAY      360
#define FADE_TIME           800
#define MARK_DELAY          2700
#define TAG_DELAY           2800
#define BREATHE_DELAY       3700
#define BREATHE_TIME        3000
#define BREATHE_OPA         LV_OPA_COVER * 45 / 100

/*Going to the next screen: the content fades out, then the new one fades in*/
#define SCREEN_FADE_TIME    200

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
} ease_t;

typedef enum {
    LETTER_RISE,    /*Rises in and stays*/
    LETTER_GROW,    /*Grows in from zero width*/
    LETTER_OUT,     /*Rises in, then shrinks away*/
} letter_kind_t;

typedef struct {
    char text[2];
    letter_kind_t kind;
    uint32_t delay;
    bool accent;
} letter_t;

typedef struct {
    lv_font_t * logo_font;
    lv_font_t * mark_font;
    lv_font_t * tag_font;
    int32_t min_side;
    lv_osptek_boot_anim_next_cb_t next_cb;
    bool ready;
} boot_anim_ctx_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static int32_t scale(const boot_anim_ctx_t * ctx, int32_t v);
static int32_t em(int32_t font_size, int32_t milli_em);
static void create_letter(boot_anim_ctx_t * ctx, lv_obj_t * row, const letter_t * l, int32_t logo_size);
static lv_obj_t * create_box(lv_obj_t * parent);
static lv_anim_t * anim_start(lv_obj_t * obj, lv_anim_exec_xcb_t exec_cb, int32_t from, int32_t to,
                              uint32_t delay, uint32_t time, const ease_t * ease);
static void anim_opa_cb(void * obj, int32_t v);
static void anim_translate_y_cb(void * obj, int32_t v);
static void anim_width_cb(void * obj, int32_t v);
static void grown_cb(lv_anim_t * a);
static void tag_shown_cb(lv_anim_t * a);
static void root_event_cb(lv_event_t * e);
static void screen_next(lv_osptek_boot_anim_next_cb_t create_cb);
static void fade_children(lv_obj_t * screen, lv_opa_t from, lv_opa_t to, lv_anim_completed_cb_t completed_cb,
                          void * user_data);
static void fade_children_cb(void * screen, int32_t v);
static void faded_out_cb(lv_anim_t * a);
static void faded_in_cb(lv_anim_t * a);

/**********************
 *  STATIC VARIABLES
 **********************/

extern const unsigned char lv_osptek_boot_anim_manrope_bold_ttf[];
extern const size_t lv_osptek_boot_anim_manrope_bold_ttf_len;
extern const unsigned char lv_osptek_boot_anim_manrope_semibold_ttf[];
extern const size_t lv_osptek_boot_anim_manrope_semibold_ttf_len;

/*CSS timing functions*/
static const ease_t ease_out_expo = {LV_BEZIER_VAL_FLOAT(0.16), LV_BEZIER_VAL_FLOAT(1), LV_BEZIER_VAL_FLOAT(0.3), LV_BEZIER_VAL_FLOAT(1)};
static const ease_t ease_in_out_cubic = {LV_BEZIER_VAL_FLOAT(0.65), LV_BEZIER_VAL_FLOAT(0), LV_BEZIER_VAL_FLOAT(0.35), LV_BEZIER_VAL_FLOAT(1)};
static const ease_t ease_out = {LV_BEZIER_VAL_FLOAT(0), LV_BEZIER_VAL_FLOAT(0), LV_BEZIER_VAL_FLOAT(0.58), LV_BEZIER_VAL_FLOAT(1)};
static const ease_t ease_in_out = {LV_BEZIER_VAL_FLOAT(0.42), LV_BEZIER_VAL_FLOAT(0), LV_BEZIER_VAL_FLOAT(0.58), LV_BEZIER_VAL_FLOAT(1)};

static const letter_t letters[] = {
    {"o", LETTER_RISE, 150, true},
    {"s", LETTER_GROW, 1850, false},
    {"p", LETTER_RISE, 220, false},
    {"t", LETTER_GROW, 1950, false},
    {"e", LETTER_RISE, 290, false},
    {"n", LETTER_OUT, OUT_RISE_DELAY, false},
    {"k", LETTER_GROW, 2050, false},
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_osptek_boot_anim_show(lv_osptek_boot_anim_next_cb_t next_cb)
{
    /*Own screen, so the next screen can fade in over it and delete it*/
    lv_obj_t * parent = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(parent, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_screen_load(parent);

    boot_anim_ctx_t * ctx = lv_malloc_zeroed(sizeof(boot_anim_ctx_t));
    LV_ASSERT_MALLOC(ctx);
    ctx->next_cb = next_cb;

    lv_obj_update_layout(parent);
    ctx->min_side = LV_MIN(lv_obj_get_content_width(parent), lv_obj_get_content_height(parent));

    int32_t logo_size = scale(ctx, LOGO_FONT_SIZE);
    int32_t mark_size = scale(ctx, MARK_FONT_SIZE);
    int32_t tag_size = LV_MAX(scale(ctx, TAG_FONT_SIZE), TAG_FONT_SIZE_MIN);
    ctx->logo_font = lv_tiny_ttf_create_data_ex(lv_osptek_boot_anim_manrope_bold_ttf,
                                                lv_osptek_boot_anim_manrope_bold_ttf_len, logo_size,
                                                LV_FONT_KERNING_NONE, 16);
    ctx->mark_font = lv_tiny_ttf_create_data_ex(lv_osptek_boot_anim_manrope_semibold_ttf,
                                                lv_osptek_boot_anim_manrope_semibold_ttf_len, mark_size,
                                                LV_FONT_KERNING_NONE, 4);
    ctx->tag_font = lv_tiny_ttf_create_data_ex(lv_osptek_boot_anim_manrope_semibold_ttf,
                                               lv_osptek_boot_anim_manrope_semibold_ttf_len, tag_size,
                                               LV_FONT_KERNING_NORMAL, 16);

    /*Root: full parent, column layout centered like the HTML flex box*/
    lv_obj_t * root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(root, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(root, false);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(root, scale(ctx, COLUMN_GAP), 0);
    lv_obj_set_user_data(root, ctx);
    lv_obj_add_event_cb(root, root_event_cb, LV_EVENT_ALL, NULL);

    /*Logo row, letters are top aligned*/
    lv_obj_t * row = create_box(root);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_overflow_visible(row, true);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    for(uint32_t i = 0; i < sizeof(letters) / sizeof(letters[0]); i++) {
        create_letter(ctx, row, &letters[i], logo_size);
    }

    /*Registered mark*/
    lv_obj_t * mark_box = create_box(row);
    lv_obj_set_overflow_visible(mark_box, true);
    lv_obj_set_size(mark_box, LV_SIZE_CONTENT, em(mark_size, LINE_HEIGHT_EM));
    lv_obj_set_style_margin_top(mark_box, scale(ctx, MARK_MARGIN_TOP), 0);
    lv_obj_set_style_margin_left(mark_box, scale(ctx, MARK_MARGIN_LEFT), 0);
    lv_obj_t * mark = lv_label_create(mark_box);
    lv_label_set_text_static(mark, "®");
    lv_obj_set_style_text_font(mark, ctx->mark_font, 0);
    lv_obj_set_style_text_color(mark, lv_color_hex(COLOR_MARK), 0);
    lv_obj_set_style_opa(mark, LV_OPA_TRANSP, 0);
    lv_obj_align(mark, LV_ALIGN_LEFT_MID, 0, 0);
    anim_start(mark, anim_opa_cb, LV_OPA_TRANSP, LV_OPA_COVER, MARK_DELAY, FADE_TIME, &ease_out);

    /*Tag line: fade in, then breathe forever*/
    lv_obj_t * tag = lv_label_create(root);
    lv_label_set_text_static(tag, "TOUCH TO BEGIN");
    lv_obj_set_style_text_font(tag, ctx->tag_font, 0);
    lv_obj_set_style_text_color(tag, lv_color_hex(COLOR_TAG), 0);
    lv_obj_set_style_text_letter_space(tag, em(tag_size, TAG_SPACING_EM), 0);
    lv_obj_set_style_opa(tag, LV_OPA_TRANSP, 0);

    anim_start(tag, anim_opa_cb, LV_OPA_TRANSP, LV_OPA_COVER, TAG_DELAY, FADE_TIME, &ease_out);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, tag);
    lv_anim_set_exec_cb(&a, anim_opa_cb);
    lv_anim_set_values(&a, LV_OPA_COVER, BREATHE_OPA);
    lv_anim_set_delay(&a, BREATHE_DELAY);
    lv_anim_set_duration(&a, BREATHE_TIME / 2);
    lv_anim_set_reverse_duration(&a, BREATHE_TIME / 2);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_early_apply(&a, false);
    lv_anim_set_path_cb(&a, lv_anim_path_custom_bezier3);
    lv_anim_set_bezier3_param(&a, ease_in_out.x1, ease_in_out.y1, ease_in_out.x2, ease_in_out.y2);
    lv_anim_set_start_cb(&a, tag_shown_cb);
    lv_anim_set_user_data(&a, ctx);
    lv_anim_start(&a);

    return parent;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*From design pixels to the display*/
static int32_t scale(const boot_anim_ctx_t * ctx, int32_t v)
{
    return (v * ctx->min_side + DESIGN_SIZE / 2) / DESIGN_SIZE;
}

static int32_t em(int32_t font_size, int32_t milli_em)
{
    int32_t v = font_size * milli_em;
    return (v >= 0 ? v + 500 : v - 500) / 1000;
}

/**
 * Every letter is a box (the HTML inline-block span) holding a label.
 * The box sets the advance and clips GROW/OUT letters while their width
 * animates, the label inside is moved and faded.
 */
static void create_letter(boot_anim_ctx_t * ctx, lv_obj_t * row, const letter_t * l, int32_t logo_size)
{
    int32_t w = lv_font_get_glyph_width(ctx->logo_font, (uint32_t)l->text[0], 0) + em(logo_size, LOGO_SPACING_EM);
    if(l->kind != LETTER_RISE) w = LV_MIN(w, em(logo_size, LETTER_MAX_W_EM));

    lv_obj_t * box = create_box(row);
    lv_obj_set_size(box, l->kind == LETTER_GROW ? 0 : w, em(logo_size, LINE_HEIGHT_EM));
    if(l->kind == LETTER_RISE) lv_obj_set_overflow_visible(box, true);

    lv_obj_t * label = lv_label_create(box);
    lv_label_set_text_static(label, l->text);
    lv_obj_set_style_text_font(label, ctx->logo_font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(l->accent ? COLOR_ACCENT : COLOR_TEXT), 0);
    lv_obj_set_style_opa(label, LV_OPA_TRANSP, 0);
    /*Some glyphs (e.g. "k") draw past their advance, leave room for them*/
    lv_obj_set_width(label, em(logo_size, 1200));
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

    int32_t rise = scale(ctx, RISE_OFFSET);
    int32_t grow = scale(ctx, GROW_OFFSET);

    if(l->kind == LETTER_GROW) {
        lv_obj_set_style_translate_y(label, grow, 0);
        lv_anim_t * a = anim_start(box, anim_width_cb, 0, w, l->delay, GROW_TIME, &ease_in_out_cubic);
        lv_anim_set_completed_cb(a, grown_cb);
        anim_start(label, anim_opa_cb, LV_OPA_TRANSP, LV_OPA_COVER, l->delay, GROW_TIME, &ease_in_out_cubic);
        anim_start(label, anim_translate_y_cb, grow, 0, l->delay, GROW_TIME, &ease_in_out_cubic);
        return;
    }

    lv_obj_set_style_translate_y(label, rise, 0);
    anim_start(label, anim_opa_cb, LV_OPA_TRANSP, LV_OPA_COVER, l->delay, RISE_TIME, &ease_out_expo);
    anim_start(label, anim_translate_y_cb, rise, 0, l->delay, RISE_TIME, &ease_out_expo);

    if(l->kind == LETTER_OUT) {
        anim_start(box, anim_width_cb, w, 0, SHRINK_DELAY, SHRINK_TIME, &ease_in_out_cubic);
        anim_start(label, anim_opa_cb, LV_OPA_COVER, LV_OPA_TRANSP, SHRINK_DELAY, SHRINK_TIME, &ease_in_out_cubic);
        anim_start(label, anim_translate_y_cb, 0, -grow, SHRINK_DELAY, SHRINK_TIME, &ease_in_out_cubic);
    }
}

static lv_obj_t * create_box(lv_obj_t * parent)
{
    lv_obj_t * box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_scrollable(box, false);
    lv_obj_set_clickable(box, false);
    return box;
}

static lv_anim_t * anim_start(lv_obj_t * obj, lv_anim_exec_xcb_t exec_cb, int32_t from, int32_t to,
                              uint32_t delay, uint32_t time, const ease_t * ease)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, exec_cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_duration(&a, time);
    /*Initial states are set by hand, so a pending animation must not override them*/
    lv_anim_set_early_apply(&a, false);
    lv_anim_set_path_cb(&a, lv_anim_path_custom_bezier3);
    lv_anim_set_bezier3_param(&a, ease->x1, ease->y1, ease->x2, ease->y2);
    return lv_anim_start(&a);
}

static void anim_opa_cb(void * obj, int32_t v)
{
    lv_obj_set_style_opa(obj, (lv_opa_t)v, 0);
}

static void anim_translate_y_cb(void * obj, int32_t v)
{
    lv_obj_set_style_translate_y(obj, v, 0);
}

static void anim_width_cb(void * obj, int32_t v)
{
    lv_obj_set_width(obj, v);
}

/*Clipping is only needed while the width animates*/
static void grown_cb(lv_anim_t * a)
{
    lv_obj_set_overflow_visible(a->var, true);
}

static void tag_shown_cb(lv_anim_t * a)
{
    boot_anim_ctx_t * ctx = lv_anim_get_user_data(a);
    ctx->ready = true;
}

static void root_event_cb(lv_event_t * e)
{
    lv_obj_t * root = lv_event_get_current_target(e);
    boot_anim_ctx_t * ctx = lv_obj_get_user_data(root);
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        if(!ctx->ready || ctx->next_cb == NULL) return;
        ctx->ready = false;   /*Ignore more taps while fading out*/
        screen_next(ctx->next_cb);
    }
    else if(code == LV_EVENT_DELETE) {
        /*The labels use the fonts, so delete them before the fonts*/
        lv_obj_clean(root);
        lv_tiny_ttf_destroy(ctx->logo_font);
        lv_tiny_ttf_destroy(ctx->mark_font);
        lv_tiny_ttf_destroy(ctx->tag_font);
        lv_free(ctx);
    }
}

/**
 * Create a new screen and fill it with `create_cb`, fade the content of the
 * current screen out, then load the new one and fade its content in. Both
 * screens have the same background, so only the content changes.
 * The current screen is deleted once the new one is loaded.
 */
static void screen_next(lv_osptek_boot_anim_next_cb_t create_cb)
{
    lv_obj_t * next = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(next, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(next, LV_OPA_COVER, 0);
    create_cb(next);

    fade_children(lv_screen_active(), LV_OPA_COVER, LV_OPA_TRANSP, faded_out_cb, next);
}

/*Fade every child (not the screen, so its background stays) with one animation*/
static void fade_children(lv_obj_t * screen, lv_opa_t from, lv_opa_t to, lv_anim_completed_cb_t completed_cb,
                          void * user_data)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, screen);
    lv_anim_set_exec_cb(&a, fade_children_cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, SCREEN_FADE_TIME);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&a, completed_cb);
    lv_anim_set_user_data(&a, user_data);
    lv_anim_start(&a);
    fade_children_cb(screen, from);
}

static void fade_children_cb(void * screen, int32_t v)
{
    uint32_t n = lv_obj_get_child_count(screen);
    for(uint32_t i = 0; i < n; i++) {
        lv_obj_set_style_opa(lv_obj_get_child(screen, i), (lv_opa_t)v, 0);
    }
}

static void faded_out_cb(lv_anim_t * a)
{
    lv_obj_t * next = lv_anim_get_user_data(a);
    /*Load first: loading deletes the animations of the new screen*/
    lv_screen_load_anim(next, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
    fade_children(next, LV_OPA_TRANSP, LV_OPA_COVER, faded_in_cb, NULL);
}

/*Fully opaque children need no layer, so drop the opa style*/
static void faded_in_cb(lv_anim_t * a)
{
    lv_obj_t * screen = a->var;
    uint32_t n = lv_obj_get_child_count(screen);
    for(uint32_t i = 0; i < n; i++) {
        lv_obj_remove_local_style_prop(lv_obj_get_child(screen, i), LV_STYLE_OPA, 0);
    }
}
