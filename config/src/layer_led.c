#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/devicetree.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

#define STRIP_NODE_2 DT_NODELABEL(led_strip_2)

#if DT_NODE_EXISTS(STRIP_NODE_2)
static const struct device *strip2 = DEVICE_DT_GET(STRIP_NODE_2);

/* レイヤーごとの発光色 (RGB値: 0〜255) */
static const struct led_rgb layer_colors[] = {
    [0] = { .r = 20, .g = 20, .b = 20 }, /* Layer 0 (デフォルト): 控えめな白 */
    [1] = { .r = 50, .g = 0,  .b = 0  }, /* Layer 1: 赤 */
    [2] = { .r = 0,  .g = 50, .b = 0  }, /* Layer 2: 緑 */
    [3] = { .r = 0,  .g = 0,  .b = 50 }, /* Layer 3 (FN): 青 */
};

static void update_led_color(uint8_t layer) {
    if (!device_is_ready(strip2)) {
        return;
    }
    struct led_rgb color = (layer < 4) ? layer_colors[layer] : layer_colors[0];
    led_strip_update_rgb(strip2, &color, 1);
}

/* 起動時の初期点灯 (Zephyr仕様に合わせて引数を void に変更) */
static int layer_led_init(void) {
    update_led_color(0);
    return 0;
}
SYS_INIT(layer_led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

/* レイヤー変更時の点灯制御 */
static int layer_led_listener(const zmk_event_t *eh) {
    uint8_t layer = zmk_keymap_highest_layer_active();
    update_led_color(layer);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(layer_led, layer_led_listener);
ZMK_SUBSCRIPTION(layer_led, zmk_layer_state_changed);
#endif

ZMK_LISTENER(layer_led, layer_led_listener);
ZMK_SUBSCRIPTION(layer_led, zmk_layer_state_changed);
#endif
