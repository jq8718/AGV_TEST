#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { P4SCAN_SENSOR_UNKNOWN, P4SCAN_SENSOR_E6, P4SCAN_SENSOR_OV9281 } p4scan_sensor_kind_t;
typedef struct {
    p4scan_sensor_kind_t kind;
    const char *name;
    const char *cal_namespace;
    uint32_t width, height;
    uint8_t address;
    uint32_t chip_id;
    uint32_t tline_ns;
    int initial_exposure;
} p4scan_sensor_profile_t;
/* Selected once during camera startup, before ISP/USB/decoder tasks exist. */
const p4scan_sensor_profile_t *p4scan_sensor_profile(void);
bool p4scan_sensor_select(uint8_t address, uint32_t chip_id);
bool p4scan_sensor_is_e6(void);
#ifdef __cplusplus
}
#endif
