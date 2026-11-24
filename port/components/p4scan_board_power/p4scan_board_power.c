#include "p4scan_board_power.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_ldo_regulator.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "agv_e6_power";
static i2c_master_dev_handle_t s_aw;
static bool s_ready;
#define VSYS_GPIO 33
#define VSYS_ON_LEVEL 0
#define VSYS_OFF_LEVEL 1
static const int s_outputs[] = {33, 21, 6, 26, 28, 20, 18, 19, 22};

static esp_err_t aw_write(uint8_t reg, uint8_t value)
{
    uint8_t data[] = {reg, value};
    return i2c_master_transmit(s_aw, data, sizeof(data), 100);
}

esp_err_t p4scan_ov01c1b_set_lights(i2c_master_bus_handle_t bus, bool aiming, bool illumination)
{
    (void)bus;
    ESP_RETURN_ON_FALSE(s_ready, ESP_ERR_INVALID_STATE, TAG, "power not ready");
    /* E6 aiming and internal illumination share the VSYS supply. Ensure the
     * rail is enabled before asserting either lamp control. */
    if (aiming || illumination) {
        ESP_RETURN_ON_ERROR(gpio_set_level(VSYS_GPIO, VSYS_ON_LEVEL), TAG, "VSYS enable before lights");
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    ESP_RETURN_ON_ERROR(gpio_set_level(18, aiming), TAG, "aim");
    /* Internal light must remain independent of external-driver I2C faults. */
    ESP_RETURN_ON_ERROR(gpio_set_level(19, illumination), TAG, "internal light");
    ESP_RETURN_ON_ERROR(gpio_set_level(22, 0), TAG, "strobe low before arming");
    if (s_aw) {
        /* Register 0x01: bit4=hardware TORCH enable, bit5=FLASH disabled,
         * mode=standby, LED1/2 enabled. GPIO22 high selects torch operation.
         * Datasheet p26 register table resolves the p18 bit-number typo. */
        ESP_RETURN_ON_ERROR(aw_write(0x01, illumination ? 0x13 : 0x80), TAG, "AW torch gate");
        ESP_RETURN_ON_ERROR(gpio_set_level(22, illumination), TAG, "strobe gate");
    }
    ESP_LOGI(TAG, "light outputs AIM18=%d INTERNAL19=%d STROBE22=%d AW=%s",
             aiming, illumination, illumination && s_aw != NULL, s_aw ? "ready" : "unavailable");
    return ESP_OK;
}

esp_err_t p4scan_ov01c1b_power_off(i2c_master_bus_handle_t bus)
{
    (void)bus;
    esp_err_t ret = ESP_OK;
    if (s_aw) {
        ret = aw_write(0x01, 0x80);
        esp_err_t err = i2c_master_bus_rm_device(s_aw);
        if (ret == ESP_OK) ret = err;
        s_aw = NULL;
    }
    const int order[] = {19, 22, 18, 20, 21, 28, 26, 6, 33};
    for (unsigned i = 0; i < sizeof(order)/sizeof(order[0]); ++i) {
        esp_err_t err = gpio_set_level(order[i], order[i] == VSYS_GPIO ? VSYS_OFF_LEVEL : 0);
        if (ret == ESP_OK) ret = err;
    }
    s_ready = false;
    return ret;
}

esp_err_t p4scan_ov01c1b_power_on(i2c_master_bus_handle_t bus)
{
    esp_err_t ret;
    uint64_t mask = 0;
    for (unsigned i = 0; i < sizeof(s_outputs)/sizeof(s_outputs[0]); ++i) {
        mask |= 1ULL << s_outputs[i];
        ESP_RETURN_ON_ERROR(gpio_set_level(s_outputs[i], s_outputs[i] == VSYS_GPIO ? VSYS_OFF_LEVEL : 0), TAG, "safe latch");
    }
    gpio_config_t outputs = {.pin_bit_mask = mask, .mode = GPIO_MODE_OUTPUT};
    ESP_RETURN_ON_ERROR(gpio_config(&outputs), TAG, "power GPIOs");
    ESP_GOTO_ON_ERROR(gpio_input_enable(33), fail, TAG, "VSYS input sense");
    /* Enable pad reads for lamp output diagnostics; output drive is unchanged. */
    ESP_GOTO_ON_ERROR(gpio_input_enable(18), fail, TAG, "aim input sense");
    ESP_GOTO_ON_ERROR(gpio_input_enable(19), fail, TAG, "internal input sense");
    ESP_GOTO_ON_ERROR(gpio_input_enable(22), fail, TAG, "strobe input sense");
    const int order[] = {33, 21, 6, 26, 28};
    for (unsigned i = 0; i < sizeof(order)/sizeof(order[0]); ++i) {
        ESP_GOTO_ON_ERROR(gpio_set_level(order[i], order[i] == VSYS_GPIO ? VSYS_ON_LEVEL : 1), fail, TAG, "rail enable");
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_GOTO_ON_ERROR(gpio_set_level(20, 1), fail, TAG, "sensor enable");
    vTaskDelay(pdMS_TO_TICKS(5));
    s_ready = true;
    i2c_device_config_t aw = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x63, .scl_speed_hz = 100000};
    ESP_GOTO_ON_ERROR(i2c_master_bus_add_device(bus, &aw, &s_aw), fail, TAG, "AW device");
    uint8_t reg = 0, id = 0;
    ret = i2c_master_transmit_receive(s_aw, &reg, 1, &id, 1, 100);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "AW36515 unavailable; external light disabled (%s)", esp_err_to_name(ret));
        i2c_master_bus_rm_device(s_aw);
        s_aw = NULL;
    } else {
        ESP_LOGI(TAG, "AW36515 ID=0x%02x", id);
        ESP_GOTO_ON_ERROR(aw_write(0x01, 0x80), fail, TAG, "AW standby");
        /* Conservative initial torch current; board optical/thermal acceptance pending. */
        ESP_GOTO_ON_ERROR(aw_write(0x05, 0x10), fail, TAG, "LED1 torch");
        ESP_GOTO_ON_ERROR(aw_write(0x06, 0x10), fail, TAG, "LED2 torch");
        ESP_GOTO_ON_ERROR(aw_write(0x07, 0x09), fail, TAG, "boost");
        ESP_GOTO_ON_ERROR(aw_write(0x08, 0x1a), fail, TAG, "timing");
    }
    ESP_LOGI(TAG, "E6 power ready; GPIO21 static oscillator enable; SCCB 0x10");
    return ESP_OK;
fail:
    p4scan_ov01c1b_power_off(bus);
    return ret;
}

esp_err_t p4scan_ov01c1b_run_light_diagnostic(i2c_master_bus_handle_t bus)
{
    ESP_RETURN_ON_ERROR(p4scan_ov01c1b_set_lights(bus, true, true), TAG, "lights on");
    vTaskDelay(pdMS_TO_TICKS(200));
    return p4scan_ov01c1b_set_lights(bus, false, false);
}

void p4scan_ov01c1b_log_sensor_state(i2c_master_bus_handle_t bus)
{
    (void)bus;
    /* Sensor driver logs paged register state; do not change its page concurrently. */
}

void p4scan_ov01c1b_log_light_state(void)
{
    esp_ldo_dump(stdout);
    ESP_LOGI(TAG, "VSYS active-low GPIO33=%d enabled=%d (digital pad; measure VSYS rail separately)",
             gpio_get_level(VSYS_GPIO), gpio_get_level(VSYS_GPIO) == VSYS_ON_LEVEL);
    gpio_dump_io_configuration(stdout, (1ULL << 33) | (1ULL << 18) | (1ULL << 19));
    ESP_LOGI(TAG, "pad reads GPIO35=%d AIM18=%d INTERNAL19=%d STROBE22=%d",
             gpio_get_level(35), gpio_get_level(18), gpio_get_level(19), gpio_get_level(22));
    if (!s_aw) { ESP_LOGW(TAG, "AW36515 unavailable"); return; }
    const uint8_t regs[] = {0x00, 0x01, 0x05, 0x06, 0x0a, 0x0b};
    for (unsigned i = 0; i < sizeof(regs); ++i) {
        uint8_t value = 0;
        esp_err_t err = i2c_master_transmit_receive(s_aw, &regs[i], 1, &value, 1, 100);
        if (err == ESP_OK) ESP_LOGI(TAG, "AW36515 reg%02x=%02x", regs[i], value);
        else ESP_LOGW(TAG, "AW36515 read reg%02x: %s", regs[i], esp_err_to_name(err));
    }
}
