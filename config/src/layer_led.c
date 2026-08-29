#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/devicetree.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

#define STRIP_NODE_2 DT_NODELABEL(led_strip_2)

#if DT_NODE_EXISTS(STRIP_NODE_2)
static const struct device *strip2 = DEVICE_DT_GET(STRIP_NODE_2);

/* レイヤーごとの発光色 (RGB値) */
static const struct led_rgb layer_colors[] = {
    [0] = { .r = 0,   .g = 0,   .b = 0   }, /* Layer 0: 消灯 */
    [1] = { .r = 50,  .g = 0,   .b = 0   }, /* Layer 1: 赤 */
    [2] = { .r = 0,   .g = 50,  .b = 0   }, /* Layer 2: 緑 */
    [3] = { .r = 0,   .g = 0,   .b = 50  }, /* Layer 3 (FN): 青 */
};

static int layer_led_listener(const zmk_event_t *eh) {
    if (!device_is_ready(strip2)) {
        return 0;
    }

    uint8_t layer = zmk_keymap_highest_layer_active();
    struct led_rgb color = (layer < 4) ? layer_colors[layer] : layer_colors[0];

    led_strip_update_rgb(strip2, &color, 1);
    return 0;
}

ZMK_LISTENER(layer_led, layer_led_listener);
ZMK_SUBSCRIPTION(layer_led, zmk_layer_state_changed);
#endif
