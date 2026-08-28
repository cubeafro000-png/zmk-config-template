#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/kscan.h>

#define DT_DRV_COMPAT zmk_kscan_duplex

struct kscan_duplex_config {
    struct gpio_dt_spec *rows;
    size_t num_rows;
    struct gpio_dt_spec *cols;
    size_t num_cols;
};

struct kscan_duplex_data {
    kscan_callback_t callback;
    const struct device *dev;
    struct k_work_delayable work;
};

static void kscan_duplex_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct kscan_duplex_data *data = CONTAINER_OF(dwork, struct kscan_duplex_data, work);
    const struct device *dev = data->dev;
    const struct kscan_duplex_config *cfg = dev->config;

    /* フェーズ1: 前半15キー (Col -> Row) */
    for (size_t c = 0; c < cfg->num_cols; c++) {
        gpio_pin_configure_dt(&cfg->cols[c], GPIO_OUTPUT_INACTIVE);
    }
    for (size_t r = 0; r < cfg->num_rows; r++) {
        gpio_pin_configure_dt(&cfg->rows[r], GPIO_INPUT | GPIO_PULL_DOWN);
    }

    for (size_t c = 0; c < cfg->num_cols; c++) {
        gpio_pin_set_dt(&cfg->cols[c], 1);
        k_busy_wait(10);
        for (size_t r = 0; r < cfg->num_rows; r++) {
            int val = gpio_pin_get_dt(&cfg->rows[r]);
            if (data->callback) {
                data->callback(dev, r, c, val > 0);
            }
        }
        gpio_pin_set_dt(&cfg->cols[c], 0);
    }

    /* フェーズ2: 後半15キー (Row -> Col) */
    for (size_t r = 0; r < cfg->num_rows; r++) {
        gpio_pin_configure_dt(&cfg->rows[r], GPIO_OUTPUT_INACTIVE);
    }
    for (size_t c = 0; c < cfg->num_cols; c++) {
        gpio_pin_configure_dt(&cfg->cols[c], GPIO_INPUT | GPIO_PULL_DOWN);
    }

    for (size_t r = 0; r < cfg->num_rows; r++) {
        gpio_pin_set_dt(&cfg->rows[r], 1);
        k_busy_wait(10);
        for (size_t c = 0; c < cfg->num_cols; c++) {
            int val = gpio_pin_get_dt(&cfg->cols[c]);
            if (data->callback) {
                data->callback(dev, r, c + cfg->num_cols, val > 0);
            }
        }
        gpio_pin_set_dt(&cfg->rows[r], 0);
    }

    k_work_schedule(&data->work, K_MSEC(10));
}

static int kscan_duplex_configure(const struct device *dev, kscan_callback_t callback) {
    struct kscan_duplex_data *data = dev->data;
    data->callback = callback;
    return 0;
}

static int kscan_duplex_enable(const struct device *dev) {
    struct kscan_duplex_data *data = dev->data;
    k_work_schedule(&data->work, K_NO_WAIT);
    return 0;
}

static int kscan_duplex_disable(const struct device *dev) {
    struct kscan_duplex_data *data = dev->data;
    k_work_cancel_delayable(&data->work);
    return 0;
}

static int kscan_duplex_init(const struct device *dev) {
    struct kscan_duplex_data *data = dev->data;
    data->dev = dev;
    k_work_init_delayable(&data->work, kscan_duplex_work_handler);
    return 0;
}

static const struct kscan_driver_api kscan_duplex_api = {
    .config = kscan_duplex_configure,
    .enable_callback = kscan_duplex_enable,
    .disable_callback = kscan_duplex_disable,
};

#define KSCAN_DUPLEX_INIT(n) \
    static struct gpio_dt_spec rows_##n[] = { \
        DT_INST_FOREACH_PROP_ELEM(n, row_gpios, GPIO_DT_SPEC_GET_BY_IDX) \
    }; \
    static struct gpio_dt_spec cols_##n[] = { \
        DT_INST_FOREACH_PROP_ELEM(n, col_gpios, GPIO_DT_SPEC_GET_BY_IDX) \
    }; \
    static const struct kscan_duplex_config config_##n = { \
        .rows = rows_##n, \
        .num_rows = ARRAY_SIZE(rows_##n), \
        .cols = cols_##n, \
        .num_cols = ARRAY_SIZE(cols_##n), \
    }; \
    static struct kscan_duplex_data data_##n; \
    DEVICE_DT_INST_DEFINE(n, kscan_duplex_init, NULL, \
        &data_##n, &config_##n, POST_KERNEL, \
        CONFIG_KSCAN_INIT_PRIORITY, &kscan_duplex_api);

DT_INST_FOREACH_STATUS_OKAY(KSCAN_DUPLEX_INIT)
