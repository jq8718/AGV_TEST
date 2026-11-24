#include "stability.h"

#include <inttypes.h>
#include <stdbool.h>
#include "aw36515.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "stability";

static void i2c_task(void *arg)
{
    (void)arg;
    uint32_t reads = 0, errors = 0, id_errors = 0;
    uint8_t chip_id = 0;
    bool reported = false;
    const TickType_t report_after = pdMS_TO_TICKS(5000);
    const TickType_t started = xTaskGetTickCount();
    while (true) {
        esp_err_t err = aw36515_read_register(AW36515_REG_CHIP_ID, &chip_id);
        ++reads;
        if (err != ESP_OK) ++errors;
        else if (chip_id != 0x30) ++id_errors;

        if (!reported && (TickType_t)(xTaskGetTickCount() - started) >= report_after) {
            ESP_LOGI(TAG, "I2C 400 kHz GPIO16/17 addr=0x63: reads=%" PRIu32
                     " errors=%" PRIu32 " ID_errors=%" PRIu32
                     " ID=0x%02x last=%s (one-time 5 s report)",
                     reads, errors, id_errors, chip_id, esp_err_to_name(err));
            reported = true;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void stability_start(void)
{
    ESP_LOGI(TAG, "I2C capture: GPIO16 SDA, GPIO17 SCL, 400 kHz, address=0x63");
    configASSERT(xTaskCreate(i2c_task, "i2c_capture", 3072, NULL, 5, NULL) == pdPASS);
}
