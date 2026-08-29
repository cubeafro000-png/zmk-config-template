#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/devicetree.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

#define STRIP_NODE_2 DT_NODELABEL(led_strip_2)

#if DT_NODE_EXISTS(STRIP_NODE_2)
static const struct device *strip2 = DEVICE_DT_GET(STRIP_NODE_2);
static struct k_work_delayable init_led_work;
static uint8_t last_layer = 255; /* 直前のレイヤー状態を保持（重複更新防止用） */

/* レイヤーごとの発光色 (0〜5の6レイヤー分定義) */
static const struct led_rgb layer_colors[] = {
    [0] = { .r = 20,  .g = 20,  .b = 20  }, /* Layer 0: 白 */
    [1] = { .r = 100, .g = 0,   .b = 0   }, /* Layer 1: 赤 */
    [2] = { .r = 0,   .g = 100, .b = 0   }, /* Layer 2: 緑 */
    [3] = { .r = 0,   .g = 0,   .b = 100 }, /* Layer 3 (FN): 青 */
    [4] = { .r = 80,  .g = 80,  .b = 0   }, /* Layer 4: 黄 */
    [5] = { .r = 80,  .g = 0,   .b = 80  }, /* Layer 5: 紫 */
};

static void update_led_color(uint8_t layer) {
    /* デバイス未準備、またはレイヤーが変化していない場合は処理を中断（無駄な割り込み停止を回避） */
    if (!device_is_ready(strip2) || layer == last_layer) {
        return;
    }
    last_layer = layer;

    struct led_rgb color = (layer < 6) ? layer_colors[layer] : layer_colors[0];
    led_strip_update_rgb(strip2, &color, 1);
}

/* 起動後 500ms 遅らせて初期点灯を実行 */
static void init_led_work_handler(struct k_work *work) {
    update_led_color(0);
}

static int layer_led_init(const struct device *dev) {
    ARG_UNUSED(dev);
    k_work_init_delayable(&init_led_work, init_led_work_handler);
    k_work_schedule(&init_led_work, K_MSEC(500));
    return 0;
}
SYS_INIT(layer_led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

/* レイヤー変更時の点灯制御 */
static int layer_led_listener(const zmk_event_t *eh) {
    struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
    if (ev != NULL) {
        uint8_t layer = zmk_keymap_highest_layer_active();
        update_led_color(layer);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(layer_led, layer_led_listener);
ZMK_SUBSCRIPTION(layer_led, zmk_layer_state_changed);
#endif
