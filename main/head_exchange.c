#include "head_exchange.h"
#include "p4scan_board_power.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#define TAG "head_exchange"
#define HEAD_BUTTON_GPIO 36

static void monitor_button(void *argument)
{
    (void)argument;
    bool raw = gpio_get_level(HEAD_BUTTON_GPIO) == 0;
    bool stable = raw;
    bool armed = false;
    int64_t changed = esp_timer_get_time();
    for (;;) {
        bool pressed = gpio_get_level(HEAD_BUTTON_GPIO) == 0;
        int64_t now = esp_timer_get_time();
        if (pressed != raw) { raw = pressed; changed = now; }
        if (now - changed >= 30000) {
            if (!raw) armed = true;
            if (stable != raw) {
                stable = raw;
                if (stable && armed) head_exchange_power_off();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static esp_err_t store_off(bool off)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("head_power", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, "off", off);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

void head_exchange_boot(void)
{
    ESP_ERROR_CHECK(p4scan_board_force_head_off());
    gpio_config_t button = {.pin_bit_mask = 1ULL << HEAD_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE};
    ESP_ERROR_CHECK(gpio_config(&button));
    /* Do not erase NVS on failure: camera calibration and the off interlock
     * must survive errors. Fail closed before camera power can be enabled. */
    ESP_ERROR_CHECK(nvs_flash_init());
    nvs_handle_t h;
    uint8_t off = 0;
    esp_err_t err = nvs_open("head_power", NVS_READONLY, &h);
    if (err == ESP_OK) {
        err = nvs_get_u8(h, "off", &off);
        nvs_close(h);
    }
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_ERROR_CHECK(p4scan_board_force_head_off());
        ESP_ERROR_CHECK(err);
    }
    if (!off) {
        ESP_ERROR_CHECK(xTaskCreatePinnedToCore(monitor_button, "head_button", 4096,
            NULL, 12, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
        return;
    }
    ESP_ERROR_CHECK(p4scan_board_force_head_off());
    gpio_set_level(4, 1);
    gpio_set_direction(4, GPIO_MODE_OUTPUT);
    gpio_set_level(53, 0);
    gpio_set_direction(53, GPIO_MODE_OUTPUT);
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGW(TAG, "HEAD OFF: VSYS33=1; EN6/26/28, MCLK21, SENSOR20, AIM18, LIGHT19/22=0; I2C16/17 high-Z. Red LED steady. GPIO36 next press restarts and detects head.");
    /* Require a stable release after reboot, so holding the first press
     * cannot immediately turn power back on. */
    bool raw = gpio_get_level(HEAD_BUTTON_GPIO) == 0;
    bool stable = raw;
    bool armed = false;
    int64_t changed = esp_timer_get_time();
    for (;;) {
        bool pressed = gpio_get_level(HEAD_BUTTON_GPIO) == 0;
        int64_t now = esp_timer_get_time();
        if (pressed != raw) { raw = pressed; changed = now; }
        if (now - changed >= 30000) {
            if (!raw) armed = true;
            if (stable != raw) {
                stable = raw;
                if (stable && armed) {
                    err = store_off(false);
                    if (err == ESP_OK) {
                        ESP_LOGI(TAG, "GPIO36 resume: restarting for fresh sensor detection and USB enumeration");
                        esp_restart();
                    }
                    ESP_LOGE(TAG, "resume state save failed: %s; head remains off", esp_err_to_name(err));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void head_exchange_power_off(void)
{
    esp_err_t err = store_off(true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "off state save failed: %s; shutdown not confirmed", esp_err_to_name(err));
        return;
    }
    ESP_ERROR_CHECK(p4scan_board_force_head_off());
    /* Hardware power drops before reset. Reboot removes CSI/IPA/decoder/USB
     * tasks together, avoiding stale frame buffers and old USB descriptors.
     * The persisted off interlock prevents camera init on the next boot. */
    esp_restart();
}
