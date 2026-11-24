#include "board_io.h"
#include "board_config.h"
#include "driver/gpio.h"
#include "esp_ldo_regulator.h"
#include "esp_check.h"
#include "esp_log.h"

void agv_board_io_init(void)
{
#if BOARD_VDDO4_MV != 0
    static esp_ldo_channel_handle_t io_ldo;
    esp_ldo_channel_config_t ldo = {.chan_id = 4, .voltage_mv = BOARD_VDDO4_MV};
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo, &io_ldo));
    ESP_LOGI("agv_board", "VDDO_4 configured=%dmV; supplies VDD_IO_5 GPIO39..48", BOARD_VDDO4_MV);
    esp_ldo_dump(stdout);
#endif
    gpio_config_t input = {.pin_bit_mask = (1ULL << 40) | (1ULL << 42),
        .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE};
    ESP_ERROR_CHECK(gpio_config(&input));
    ESP_ERROR_CHECK(gpio_set_level(41, 0));
    gpio_config_t output = {.pin_bit_mask = 1ULL << 41, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&output));
    gpio_config_t exposure = {.pin_bit_mask = 1ULL << 23,
        .mode = GPIO_MODE_INPUT, .pull_down_en = GPIO_PULLDOWN_ENABLE};
    ESP_ERROR_CHECK(gpio_config(&exposure));
}
