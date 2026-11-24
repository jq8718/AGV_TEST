#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "board_config.h"
#include "esp_chip_info.h"
#include "esp_clock_output.h"
#include "esp_clk_tree.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_private/esp_clk.h"
#include "soc/rtc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "aw36515.h"
#include "stability.h"

#define LED_OK_GPIO       GPIO_NUM_4
#define LED_ERR_GPIO      GPIO_NUM_53
#define BUTTON_GPIO       GPIO_NUM_35
#define POLL_INTERVAL_MS  10
#define DEBOUNCE_MS       30
#define RED_TOGGLE_MS     500

static const char *TAG = "gpio_demo";
static uint32_t s_mclk_test_hz;

static void start_mclk_test(void)
{
#if BOARD_MCLK_TEST_ENABLE
    static esp_clock_output_mapping_handle_t mclk;
    uint32_t source_hz = 0;
    ESP_ERROR_CHECK(esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_SPLL,
                    ESP_CLK_TREE_SRC_FREQ_PRECISION_EXACT, &source_hz));
    if (source_hz != 480000000U) {
        ESP_LOGE(TAG, "MCLK test requires 480 MHz SPLL; got %" PRIu32 " Hz", source_hz);
        return;
    }
    /* Hold the pad low until the clock divider and routing are ready. */
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_21, 0));
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_21, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_hold_en(GPIO_NUM_21));
    ESP_ERROR_CHECK(esp_clock_output_start(CLKOUT_SIG_SPLL, GPIO_NUM_21, &mclk));
    ESP_ERROR_CHECK(esp_clock_output_set_divider(mclk, 20));
    ESP_ERROR_CHECK(gpio_hold_dis(GPIO_NUM_21));
    s_mclk_test_hz = source_hz / 20;
    ESP_LOGI(TAG, "MCLK test: GPIO21 output enabled, SPLL=%" PRIu32
             " Hz /20 = %" PRIu32 " Hz (verify pad with scope)", source_hz, source_hz / 20);
#endif
}

static void configure_io_power(void)
{
#if BOARD_VDDO4_MV != 0
    static esp_ldo_channel_handle_t vddo4;
    const esp_ldo_channel_config_t config = {
        .chan_id = 4,
        .voltage_mv = BOARD_VDDO4_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&config, &vddo4));
#endif
}

static void print_startup_info(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    ESP_LOGI(TAG, "=== AGV GPIO demo startup ===");
    ESP_LOGI(TAG, "ESP32-P4 revision v%d.%d, %d cores, ESP-IDF %s, reset reason %d",
             chip.revision / 100, chip.revision % 100, chip.cores,
             esp_get_idf_version(), (int)esp_reset_reason());
    ESP_LOGI(TAG, "Console: USB Serial/JTAG (GPIO24/25)");
    ESP_LOGI(TAG, "Main crystal: %d MHz", CONFIG_XTAL_FREQ);
    ESP_LOGI(TAG, "GPIO21 MCLK test: configured=%" PRIu32 " Hz (0=inactive; measure waveform at pad)",
             s_mclk_test_hz);
    const soc_rtc_slow_clk_src_t rtc_source = rtc_clk_slow_src_get();
    const bool external_rtc = rtc_source == SOC_RTC_SLOW_CLK_SRC_XTAL32K;
    ESP_LOGI(TAG, "RTC slow clock actual source: %s (source id=%d)",
             external_rtc ? "external 32.768 kHz crystal" : "NOT external crystal",
             (int)rtc_source);
    const uint32_t rtc_cal = esp_clk_slowclk_cal_get();
    if (rtc_cal != 0) {
        const uint32_t rtc_hz = (uint32_t)((1000000ULL << RTC_CLK_CAL_FRACT) / rtc_cal);
        ESP_LOGI(TAG, "RTC slow clock boot-calibrated frequency: %" PRIu32 " Hz", rtc_hz);
    }
    if (!external_rtc) {
        ESP_LOGW(TAG, "External RTC crystal requested but not selected; check crystal startup/hardware");
    }
    ESP_LOGI(TAG, "Internal RAM heap: total=%zu, free=%zu, largest=%zu bytes",
             heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (esp_psram_is_initialized()) {
        ESP_LOGI(TAG, "PSRAM: detected %zu MB",
                 esp_psram_get_size() / (1024 * 1024));
        ESP_LOGI(TAG, "PSRAM heap: total=%zu, free=%zu bytes",
                 heap_caps_get_total_size(MALLOC_CAP_SPIRAM),
                 heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    } else {
        ESP_LOGE(TAG, "PSRAM: initialization failed or chip not found");
    }
    const bool heap_ok = heap_caps_check_integrity_all(true);
    ESP_LOGI(TAG, "RAM heap integrity: %s", heap_ok ? "PASS" : "FAIL");

    uint32_t flash_id = 0;
    uint32_t physical_size = 0;
    uint32_t configured_size = 0;
    esp_err_t err = esp_flash_read_id(NULL, &flash_id);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Flash JEDEC ID: 0x%06" PRIx32, flash_id);
    } else {
        ESP_LOGE(TAG, "Flash ID read failed: %s", esp_err_to_name(err));
    }
    err = esp_flash_get_physical_size(NULL, &physical_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Flash physical size: %" PRIu32 " MB (%" PRIu32 " bytes)",
                 physical_size / (1024 * 1024), physical_size);
    } else {
        ESP_LOGE(TAG, "Flash size detection failed: %s", esp_err_to_name(err));
    }
    err = esp_flash_get_size(NULL, &configured_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Flash configured size: %" PRIu32 " MB", configured_size / (1024 * 1024));
    } else {
        ESP_LOGE(TAG, "Flash configured size read failed: %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "LDO channels acquired by this application:");
    ESP_ERROR_CHECK(esp_ldo_dump(stdout));
    ESP_LOGI(TAG, "VO3: not configured by demo; VO4 macro: %d mV (0 = preserve boot state)",
             BOARD_VDDO4_MV);
    ESP_LOGI(TAG, "LDO table shows driver requests, not measured pin voltages");
    fflush(stdout);
}

/* Both LEDs are active low: 0 = on, 1 = off. */
static void set_led(gpio_num_t pin, bool on)
{
    ESP_ERROR_CHECK(gpio_set_level(pin, on ? 0 : 1));
}

void app_main(void)
{
    configure_io_power();
    start_mclk_test();
    /* Preload the off level before enabling the output drivers. */
    set_led(LED_OK_GPIO, false);
    set_led(LED_ERR_GPIO, false);

    const gpio_config_t led_config = {
        .pin_bit_mask = (1ULL << LED_OK_GPIO) | (1ULL << LED_ERR_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led_config));

    const gpio_config_t button_config = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&button_config));

    /* Give the USB host time to reconnect after reset. */
    vTaskDelay(pdMS_TO_TICKS(1500));
    print_startup_info();
    esp_err_t aw_err = aw36515_init();
    ESP_LOGI(TAG, "AW36515 initialization: %s", esp_err_to_name(aw_err));
    stability_start();

    const TickType_t poll_ticks = pdMS_TO_TICKS(POLL_INTERVAL_MS);
    const TickType_t debounce_ticks = pdMS_TO_TICKS(DEBOUNCE_MS);
    const TickType_t red_toggle_ticks = pdMS_TO_TICKS(RED_TOGGLE_MS);
    TickType_t wake_time = xTaskGetTickCount();
    TickType_t candidate_since = wake_time;
    TickType_t last_red_toggle = wake_time;
    int candidate_level = gpio_get_level(BUTTON_GPIO);
    int stable_level = 1;
    bool red_on = false;

    ESP_LOGI(TAG, "Red: GPIO53, 500 ms toggle; green: GPIO4, on while GPIO35 is pressed");

    while (true) {
        const TickType_t now = xTaskGetTickCount();
        const int level = gpio_get_level(BUTTON_GPIO);

        if (level != candidate_level) {
            candidate_level = level;
            candidate_since = now;
        }
        if (candidate_level != stable_level &&
            (TickType_t)(now - candidate_since) >= debounce_ticks) {
            stable_level = candidate_level;
            set_led(LED_OK_GPIO, stable_level == 0);
            ESP_LOGI(TAG, "Button %s", stable_level == 0 ? "pressed" : "released");
        }

        if ((TickType_t)(now - last_red_toggle) >= red_toggle_ticks) {
            last_red_toggle = now;
            red_on = !red_on;
            set_led(LED_ERR_GPIO, red_on);
        }

        vTaskDelayUntil(&wake_time, poll_ticks);
    }
}
