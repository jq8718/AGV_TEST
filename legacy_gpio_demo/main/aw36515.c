#include "aw36515.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_SDA_GPIO GPIO_NUM_16
#define I2C_SCL_GPIO GPIO_NUM_17
#define STROBE_GPIO GPIO_NUM_22
#define AW36515_I2C_HZ 400000

static const char *TAG = "aw36515";
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;

static esp_err_t aw_write(uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val};
    return i2c_master_transmit(s_dev, data, sizeof(data), 100);
}

static esp_err_t aw_read(uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, 100);
}

esp_err_t aw36515_read_register(uint8_t reg, uint8_t *value)
{
    if (s_dev == NULL || value == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return aw_read(reg, value);
}

esp_err_t aw36515_init(void)
{
    gpio_config_t strobe = {
        .pin_bit_mask = 1ULL << STROBE_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&strobe), TAG, "STROBE config failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(STROBE_GPIO, 0), TAG, "STROBE low failed");

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), TAG, "I2C bus init failed");
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AW36515_I2C_ADDR,
        .scl_speed_hz = AW36515_I2C_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "I2C: SCL=%d Hz, SDA=GPIO16 SCL=GPIO17, internal pull-ups enabled", AW36515_I2C_HZ);

    /* VIN must remain above VPOR for >=2 ms before the first command. */
    vTaskDelay(pdMS_TO_TICKS(3));
    uint8_t chip_id = 0;
    err = aw_read(AW36515_REG_CHIP_ID, &chip_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "AW36515 not responding at 0x%02x: %s", AW36515_I2C_ADDR, esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "AW36515 found at 7-bit I2C address 0x%02x, chip ID 0x%02x", AW36515_I2C_ADDR, chip_id);

    /* Standby, both LED channels off; brightness and timing are explicit. */
    ESP_RETURN_ON_ERROR(aw_write(AW36515_REG_ENABLE, 0x80), TAG, "enable write failed");
    ESP_RETURN_ON_ERROR(aw_write(AW36515_REG_LED1_FLASH, 0x7F), TAG, "LED1 flash write failed");
    ESP_RETURN_ON_ERROR(aw_write(AW36515_REG_LED2_FLASH, 0x7F), TAG, "LED2 flash write failed");
    ESP_RETURN_ON_ERROR(aw_write(AW36515_REG_LED1_TORCH, 0x7F), TAG, "LED1 torch write failed");
    ESP_RETURN_ON_ERROR(aw_write(AW36515_REG_LED2_TORCH, 0x7F), TAG, "LED2 torch write failed");
    ESP_RETURN_ON_ERROR(aw_write(AW36515_REG_BOOST_CONFIG, 0x09), TAG, "boost config write failed");
    ESP_RETURN_ON_ERROR(aw_write(AW36515_REG_TIMING_CONFIG, 0x1A), TAG, "timing config write failed");
    ESP_LOGI(TAG, "Configured standby; flash/torch code=0x7f, boost=0x09 (2.8 A limit, 2 MHz), timing=0x1a");
    return ESP_OK;
}
