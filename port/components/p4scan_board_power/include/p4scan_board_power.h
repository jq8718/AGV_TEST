#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t p4scan_ov01c1b_power_on(i2c_master_bus_handle_t bus_handle);
esp_err_t p4scan_ov01c1b_set_lights(i2c_master_bus_handle_t bus_handle,
                                    bool aiming,
                                    bool illumination);
esp_err_t p4scan_ov01c1b_run_light_diagnostic(i2c_master_bus_handle_t bus_handle);
esp_err_t p4scan_ov01c1b_power_off(i2c_master_bus_handle_t bus_handle);
void p4scan_ov01c1b_log_light_state(void);
void p4scan_ov01c1b_log_sensor_state(i2c_master_bus_handle_t bus_handle);

#ifdef __cplusplus
}
#endif
