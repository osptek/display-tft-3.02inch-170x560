# lv_osptek_boot_anim

OSPTEK 开机动画，LVGL（≥ 9.6）组件，不依赖其他组件。

`o p e n` 依次上浮 → `n` 缩没、`s t k` 长出变成 `osptek` → `®` 与 `TOUCH TO BEGIN` 淡入，标语持续呼吸。
约 3.7 s 后点击屏幕，动画淡出，下一个界面淡入。

按 480×480 设计稿绘制，运行时以 `min(宽, 高) / 480` 缩放，换任何分辨率的屏都能直接用。纯黑底，适合 AMOLED。

## 使用

1. 把组件放进工程（任选一种）：
   - 拷贝整个目录到工程的 `components/` 下；
   - 顶层 `CMakeLists.txt` 里 `set(EXTRA_COMPONENT_DIRS <路径>/lv_osptek_boot_anim)`；
   - `main/idf_component.yml` 里加 `lv_osptek_boot_anim: { path: <路径>/lv_osptek_boot_anim }`。
2. 不用改 menuconfig：组件会自动打开 LVGL 的 TinyTTF 和 Flex。
3. `#include "lv_osptek_boot_anim.h"`，在 LVGL 锁内调用：

```c
static void home_create(lv_obj_t *screen)
{
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "Home");
    lv_obj_center(label);
}

lvgl_lock(-1);
lv_osptek_boot_anim_show(home_create);   /* 传 NULL 则停在开机动画 */
lvgl_unlock();
```

`lv_osptek_boot_anim_show()` 新建并加载一个屏幕，返回这个屏幕。点击后会新建下一个屏幕（黑色背景）交给
`home_create` 填内容，切换完成后删除动画屏幕。

非 ESP-IDF 平台（如 PC 模拟器）：把 `include/` 加进头文件路径，编译 `src/` 下的 3 个 .c，
并在 `lv_conf.h` 里打开 `LV_USE_TINY_TTF` 和 `LV_USE_FLEX`。

## 调整

颜色、字号、各段动画的时间都在 `src/lv_osptek_boot_anim.c` 开头的宏里。背景色 `COLOR_BG` 同时也是下一个屏幕的背景色。

TinyTTF 运行时生成字形，建议用 `CONFIG_LV_USE_CLIB_MALLOC` + PSRAM。

## 字体

`src/fonts/` 里是 Manrope 的 `.ttf` 原始字节，只裁剪了动画用到的字符，两个文件合计约 5 KB：

| 文件 | 内容 |
|---|---|
| `manrope_bold.c` | Manrope 700：`o p e n s t k` |
| `manrope_semibold.c` | Manrope 600：`®` 和 `TOUCH TO BEGIN` |

改文字时要重新裁剪：用 fonttools 从 Google Fonts 的 `Manrope[wght].ttf` 固定字重（`instancer`）后
用 `pyftsubset` 只保留需要的字符，再转成 C 数组，数组名保持 `lv_osptek_boot_anim_manrope_*_ttf`。

## 授权

- Manrope 字体使用 SIL OFL 1.1，分发时须附带 `src/fonts/OFL.txt`。
