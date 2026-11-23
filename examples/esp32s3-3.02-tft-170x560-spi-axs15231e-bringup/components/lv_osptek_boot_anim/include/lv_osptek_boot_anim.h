/*
 * SPDX-FileCopyrightText: Copyright 2026 OSPTEK
 * SPDX-License-Identifier: CC-BY-4.0
 */

/**
 * OSPTEK boot animation: the osptek logo builds up letter by letter, then
 * "TOUCH TO BEGIN" breathes until the screen is tapped.
 *
 * Drawn from a 480x480 design and scaled by min(width, height) / 480 of the
 * display, so any resolution works. Needs LVGL >= 9.6 with TinyTTF and Flex.
 */

#ifndef LV_OSPTEK_BOOT_ANIM_H
#define LV_OSPTEK_BOOT_ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/** Fills `screen` (a new, empty screen with a black background) with the UI after the animation */
typedef void (*lv_osptek_boot_anim_next_cb_t)(lv_obj_t * screen);

/**
 * Show the boot animation on a new screen and load it.
 * Once "TOUCH TO BEGIN" is shown, a tap creates a new screen, fills it with
 * `next_cb`, fades the animation out and the new screen in, and deletes the
 * animation screen. Call it from the LVGL task, or with the LVGL lock held.
 * @param next_cb   fills the next screen, NULL: stay on the animation
 * @return          the animation screen
 */
lv_obj_t * lv_osptek_boot_anim_show(lv_osptek_boot_anim_next_cb_t next_cb);

#ifdef __cplusplus
}
#endif

#endif /*LV_OSPTEK_BOOT_ANIM_H*/
