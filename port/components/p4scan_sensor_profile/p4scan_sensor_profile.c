#include "p4scan_sensor_profile.h"
#include "sdkconfig.h"
static const p4scan_sensor_profile_t unknown = { .name = "unidentified camera" };
static const p4scan_sensor_profile_t e6 = {
    P4SCAN_SENSOR_E6, "E6 OV01C1B", "scan_pose", 1024, 1024, 0x10, 0x56014311, 4090, 1549
};
static const p4scan_sensor_profile_t ov9281 = {
    P4SCAN_SENSOR_OV9281, "OV9281", "pose_ov9281", 1280, 720, 0x60, 0x9281, 10989, 681
};
#if CONFIG_P4SCAN_SENSOR_AUTO
static const p4scan_sensor_profile_t *selected = &unknown;
#elif CONFIG_P4SCAN_SENSOR_OV9281
static const p4scan_sensor_profile_t *selected = &ov9281;
#else
static const p4scan_sensor_profile_t *selected = &e6;
#endif
const p4scan_sensor_profile_t *p4scan_sensor_profile(void) { return selected; }
bool p4scan_sensor_is_e6(void) { return selected->kind == P4SCAN_SENSOR_E6; }
bool p4scan_sensor_select(uint8_t address, uint32_t chip_id)
{
    const p4scan_sensor_profile_t *candidate = &unknown;
    if (address == e6.address && chip_id == e6.chip_id) candidate = &e6;
    if (address == ov9281.address && chip_id == ov9281.chip_id) candidate = &ov9281;
#if CONFIG_P4SCAN_SENSOR_E6
    if (candidate != &e6) return false;
#elif CONFIG_P4SCAN_SENSOR_OV9281
    if (candidate != &ov9281) return false;
#endif
    selected = candidate;
    return candidate != &unknown;
}
