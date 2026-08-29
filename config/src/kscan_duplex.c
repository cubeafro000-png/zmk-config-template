#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/kscan.h>

#define DT_DRV_COMPAT zmk_kscan_duplex

#define MATRIX_ROWS 5
#define MATRIX_COLS 6

struct kscan_duplex_config {
    struct gpio_dt_spec row_gpios[5];
    struct gpio_dt_spec col_gpios[3];
};

struct kscan_duplex_data {
    kscan_callback_t callback;
    const struct device *dev;
    struct k_work_delayable work;
    bool matrix_state[MATRIX_ROWS][MATRIX_COLS];
};

static void kscan_duplex_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct kscan_duplex_data *data = CONTAINER_OF(dwork, struct kscan_duplex_data, work);
    const struct device *dev = data->dev;
    const struct kscan_duplex_config *config = dev->config;

    if (!data->callback) {
        k_work_schedule(&data->work, K_MSEC(10));
        return;
    }

    /* --- Phase 1: 順方向スキャン (Row -> Col) --- */
    for (int r = 0; r < 5; r++) {
        gpio_pin_configure_dt(&config->row_gpios[r], GPIO_OUTPUT_ACTIVE);
    }
    for (int c = 0; c < 3; c++) {
        gpio_pin_configure_dt(&config->col_gpios[c], GPIO_INPUT | GPIO_PULL_DOWN);
    }

    k_busy_wait(10); /* 信号安定待ち（わずか10微小秒に抑制） */

    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 3; c++) {
            bool pressed = gpio_pin_get_dt(&config->col_gpios[c]) > 0;
            int col_idx = c * 2; /* 第0, 2, 4列 */

            if (pressed != data->matrix_state[r][col_idx]) {
                data->matrix_state[r][col_idx] = pressed;
                data->callback(dev, r, col_idx, pressed);
            }
        }
    }

    /* --- Phase 2: 逆方向スキャン (Col -> Row) --- */
    for (int c = 0; c < 3; c++) {
        gpio_pin_configure_dt(&config->col_gpios[c], GPIO_OUTPUT_ACTIVE);
    }
    for (int r = 0; r < 5; r++) {
        gpio_pin_configure_dt(&config->row_gpios[r], GPIO_INPUT | GPIO_PULL_DOWN);
    }

    k_busy_wait(10); /* 信号安定待ち（10微小秒） */

    for (int c = 0; c < 3; c++) {
        for (int r = 0; r < 5; r++) {
            bool pressed = gpio_pin_get_dt(&config->row_gpios[r]) > 0;
            int col_idx = c * 2 + 1; /* 第1, 3, 5列 */

            if (pressed != data->matrix_state[r][col_idx]) {
                data->matrix_state[r][col_idx] = pressed;
                data->callback(dev, r, col_idx, pressed);
            }
        }
    }

    /* ピンの初期化（ハイ・インピーダンス解除） */
    for (int r = 0; r < 5; r++) {
        gpio_pin_configure_dt(&config->row_gpios[r], GPIO_INPUT);
    }
    for (int c = 0; c < 3; c++) {
        gpio_pin_configure_dt(&config->col_gpios[c], GPIO_INPUT);
    }

    /* CPUとUSBスレッドへ処理を譲るため、10ms周期で次回走査をスケジュール */
    k_work_schedule(&data->work, K_MSEC(10));
}

static int kscan_duplex_configure(const struct device *dev, kscan_callback_t callback) {
    struct kscan_duplex_data *data = dev->data;
    if (!callback) {
        return -EINVAL;
    }
    data->callback = callback;
    return 0;
}

static int kscan_duplex_enable(const struct device *dev) {
    struct kscan_duplex_data *data = dev->data;
    k_work_schedule(&data->work, K_MSEC(10));
    return 0;
}

static int kscan_duplex_disable(const struct device *dev) {
    struct kscan_duplex_data *data = dev->data;
    k_work_cancel_delayable(&data->work);
    return 0;
}

static int kscan_duplex_init(const struct device *dev) {
    struct kscan_duplex_data *data = dev->data;
    const struct kscan_duplex_config *config = dev->config;

    data->dev = dev;
    k_work_init_delayable(&data->work, kscan_duplex_work_handler);

    for (int r = 0; r < 5; r++) {
        if (!device_is_ready(config->row_gpios[r].port)) return -ENODEV;
        gpio_pin_configure_dt(&config->row_gpios[r], GPIO_INPUT);
    }
    for (int c = 0; c < 3; c++) {
        if (!device_is_ready(config->col_gpios[c].port)) return -ENODEV;
        gpio_pin_configure_dt(&config->col_gpios[c], GPIO_INPUT);
    }

    return 0;
}

static const struct kscan_driver_api kscan_duplex_api = {
    .config = kscan_duplex_configure,
    .enable_callback = kscan_duplex_enable,
    .disable_callback = kscan_duplex_disable,
};

#define KSCAN_DUPLEX_INIT(n)                                                   \
    static struct kscan_duplex_data kscan_duplex_data_##n;                    \
    static const struct kscan_duplex_config kscan_duplex_config_##n = {        \
        .row_gpios = {                                                         \
            GPIO_DT_SPEC_GET_BY_IDX(DT_DRV_INST(n), row_gpios, 0),             \
            GPIO_DT_SPEC_GET_BY_IDX(DT_DRV_INST(n), row_gpios, 1),             \
            GPIO_DT_SPEC_GET_BY_IDX(DT_DRV_INST(n), row_gpios, 2),             \
            GPIO_DT_SPEC_GET_BY_IDX(DT_DRV_INST(n), row_gpios, 3),             \
            GPIO_DT_SPEC_GET_BY_IDX(DT_DRV_INST(n), row_gpios, 4),             \
        },                                                                     \
        .col_gpios = {                                                         \
            GPIO_DT_SPEC_GET_BY_IDX(DT_DRV_INST(n), col_gpios, 0),             \
            GPIO_DT_SPEC_GET_BY_IDX(DT_DRV_INST(n), col_gpios, 1),             \
            GPIO_DT_SPEC_GET_BY_IDX(DT_DRV_INST(n), col_gpios, 2),             \
        },                                                                     \
    };                                                                         \
    DEVICE_DT_INST_DEFINE(n, kscan_duplex_init, NULL,                          \
                          &kscan_duplex_data_##n,                              \
                          &kscan_duplex_config_##n,                            \
                          POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY,      \
                          &kscan_duplex_api);

DT_INST_FOREACH_STATUS_OKAY(KSCAN_DUPLEX_INIT)
