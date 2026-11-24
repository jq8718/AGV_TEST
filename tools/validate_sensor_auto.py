"""Compile actual sensor probing/profile and UVC descriptor patch code with mock I2C."""
import json
import ctypes
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    folder = ROOT / 'build_auto/sensor_validation'
    folder.mkdir(parents=True, exist_ok=True)
    (folder / 'sdkconfig.h').write_text('#define CONFIG_P4SCAN_SENSOR_AUTO 1\n')
    capture = (ROOT / 'port/components/p4scan_camera_capture/p4scan_camera_capture.c').read_text()
    probe = capture[capture.index('static esp_err_t detect_sensor('):capture.index('void p4scan_camera_config_default(')]
    usb = (ROOT / 'port/components/usb_device_uvc/tusb/usb_descriptors.c').read_text()
    patch = usb[usb.index('static void patch_sensor_frame('):usb.index('uint8_t const *tud_descriptor_configuration_cb(')]
    source = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "p4scan_sensor_profile.h"
#include "sdkconfig.h"
typedef int esp_err_t;
typedef int i2c_master_bus_handle_t;
typedef int i2c_master_dev_handle_t;
typedef struct { int dev_addr_length, device_address, scl_speed_hz; } i2c_device_config_t;
#define I2C_ADDR_BIT_LEN_7 7
#define ESP_OK 0
#define ESP_ERR_NOT_FOUND 1
#define ESP_ERR_NOT_SUPPORTED 2
#define TAG "test"
#define UVC_CAM1_FRAME_RATE 30
#define ESP_LOGI(...) ((void)0)
#define ESP_RETURN_ON_ERROR(expr,...) do { int e=(expr); if(e) return e; } while(0)
#define ESP_RETURN_ON_FALSE(expr,err,...) do { if(!(expr)) return err; } while(0)
static int present, probe_error, read_error, removed, devices, page_set;
static uint32_t chip_id;
static int i2c_master_probe(int bus, int address, int timeout) {
    if(probe_error) return probe_error;
    return (present & (address == 0x10 ? 1 : 2)) ? ESP_OK : ESP_ERR_NOT_FOUND;
}
static int i2c_master_bus_add_device(int bus, const i2c_device_config_t *cfg, int *dev) {
    assert(cfg->scl_speed_hz==100000 && cfg->dev_addr_length==7);
    *dev=cfg->device_address; devices++; return 0;
}
static int i2c_master_bus_rm_device(int dev) { removed++; return 0; }
static int i2c_master_transmit(int dev, const uint8_t *data, size_t n, int timeout) {
    assert(dev==0x10 && n==2 && data[0]==0xfd && data[1]==0); page_set=1; return 0;
}
static int i2c_master_transmit_receive(int dev, const uint8_t *reg, size_t n, uint8_t *value, size_t size, int timeout) {
    if(read_error) return read_error;
    assert(size==1);
    if(dev==0x10) { assert(page_set && n==1 && *reg<4); *value=(uint8_t)(chip_id >> (8*(3-*reg))); }
    else { assert(n==2 && reg[0]==0x30 && (reg[1]==10 || reg[1]==11)); *value=(uint8_t)(chip_id >> (8*(11-reg[1]))); }
    return 0;
}
'''
    source += probe + patch + r'''
static void check_descriptor(int ov) {
    assert(p4scan_sensor_select(ov?0x60:0x10,ov?0x9281:0x56014311));
    uint8_t d[32]={2,0,30,0x24,7,1,0};
    patch_sensor_frame(d,sizeof(d));
    unsigned w=d[7]|d[8]<<8, h=d[9]|d[10]<<8;
    assert(w==(ov?1280:1024) && h==(ov?720:1024));
    uint32_t bitrate=0, capacity=0;
    memcpy(&bitrate,d+11,4); memcpy(&capacity,d+19,4);
    assert(bitrate==w*h*16*30 && capacity==w*h*2);
    assert(strcmp(p4scan_sensor_profile()->cal_namespace,ov?"pose_ov9281":"scan_pose")==0);
    assert(p4scan_sensor_is_e6()==!ov);
}
__declspec(dllexport) int run_validation(void) {
    assert(p4scan_sensor_profile()->kind==P4SCAN_SENSOR_UNKNOWN);
    assert(!p4scan_sensor_select(0x10,0x9281));
    assert(!p4scan_sensor_select(0x60,0x56014311));
    assert(!p4scan_sensor_select(0x11,0x56014311));
    present=0; assert(detect_sensor(0)==ESP_ERR_NOT_FOUND);
    present=3; assert(detect_sensor(0)==ESP_ERR_NOT_FOUND);
    present=1; chip_id=0x56014311; assert(detect_sensor(0)==0);
    assert(p4scan_sensor_is_e6()); check_descriptor(0);
    present=2; chip_id=0x9281; assert(detect_sensor(0)==0);
    assert(!p4scan_sensor_is_e6()); check_descriptor(1);
    chip_id=0xffff; assert(detect_sensor(0)==ESP_ERR_NOT_SUPPORTED);
    read_error=8; assert(detect_sensor(0)==8); read_error=0;
    probe_error=9; assert(detect_sensor(0)==9);
    assert(devices==removed);
    puts("PASS: identities, absent/ambiguous sensor, bus errors, cleanup, native UVC dimensions and calibration namespaces");
    return 0;
}
'''
    (folder / 'probe.c').write_text(source)
    vswhere = Path('C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe')
    vs = subprocess.check_output([str(vswhere), '-latest', '-property', 'installationPath'], text=True).strip()
    profile = ROOT / 'port/components/p4scan_sensor_profile'
    batch = folder / 'compile.cmd'
    batch.write_text(f'@echo off\ncall "{vs}/VC/Auxiliary/Build/vcvars64.bat" >nul\n'
        f'cl /nologo /O2 /std:c11 /LD /I"{folder}" /I"{profile / "include"}" '
        f'"{folder / "probe.c"}" "{profile / "p4scan_sensor_profile.c"}" /link /OUT:"{folder / "probe.dll"}"\n')
    build = subprocess.run(['cmd', '/c', str(batch)], cwd=folder, capture_output=True, text=True, errors='replace')
    if build.returncode:
        raise RuntimeError(build.stdout + build.stderr)
    library = ctypes.CDLL(str(folder / 'probe.dll'))
    assert library.run_validation() == 0
    result = 'PASS: sensor identity, missing/ambiguous sensor, I2C errors, cleanup, UVC dimensions, calibration namespaces'
    print(result)
    (ROOT / 'docs/sensor_auto_host_validation.json').write_text(json.dumps({
        'passed': True, 'actual_sources': ['p4scan_sensor_profile.c', 'detect_sensor', 'patch_sensor_frame'],
        'i2c': 'mocked; does not replace hardware validation', 'result': result}, indent=2))


if __name__ == '__main__':
    main()
