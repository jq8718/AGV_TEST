"""Compile real reference/current pose and UART C code; compare measurements and wire bytes."""
import ctypes
import hashlib
import json
from pathlib import Path
import subprocess

import cv2
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = Path('D:/XC/2026/P4SCAN/ESP32P4_XCF_OV9281')


def compile_library(name, source, width=1280, height=720):
    folder = ROOT / 'build_auto/pose_uart_parity' / name
    folder.mkdir(parents=True, exist_ok=True)
    stubs = {
        'stdatomic.h': '''#pragma once
/* Single-thread host harness; task scheduling is mocked. */
typedef int atomic_int; typedef unsigned atomic_uint;
#define atomic_load(p) (*(p))
#define atomic_store(p,v) (*(p)=(v))
#define atomic_fetch_add(p,v) ((*(p)+=(v))-(v))
''',
        'esp_err.h': '''#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_INVALID_STATE 2
#define ESP_ERR_NVS_NOT_FOUND 3
#define ESP_ERR_NO_MEM 4
static inline const char *esp_err_to_name(int e){return "mock";}
''',
        'p4scan_camera_capture.h': f'''#pragma once
#define CONFIG_P4SCAN_SENSOR_AUTO 1
#define P4SCAN_CAMERA_DEFAULT_WIDTH {width}
#define P4SCAN_CAMERA_DEFAULT_HEIGHT {height}
#define P4SCAN_CAMERA_CAL_NAMESPACE "{'pose_ov9281' if width == 1280 else 'scan_pose'}"
''',
        'esp_log.h': '#pragma once\n#define ESP_LOGI(...) ((void)0)\n#define ESP_LOGW(...) ((void)0)\n',
        'esp_timer.h': '#pragma once\nstatic inline long long esp_timer_get_time(void){return 1000;}\n',
        'freertos/FreeRTOS.h': '''#pragma once
#include <stddef.h>
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(...) ((void)0)
#define portEXIT_CRITICAL(...) ((void)0)
#define pdMS_TO_TICKS(x) (x)
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY 0xffffffff
''',
        'freertos/semphr.h': '''#pragma once
typedef void *SemaphoreHandle_t;
static inline void*xSemaphoreCreateMutex(void){return (void*)1;}
static inline int xSemaphoreTake(void*h,unsigned t){return 1;}
static inline void xSemaphoreGive(void*h){}
''',
        'freertos/queue.h': '''#pragma once
#include <string.h>
typedef void *QueueHandle_t;
static unsigned packet_size; static unsigned char packet_bytes[4096];
static inline void*xQueueCreate(unsigned n,unsigned size){packet_size=size;return (void*)1;}
static inline int xQueueOverwrite(void*q,const void*p){memcpy(packet_bytes,p,packet_size);return 1;}
static inline int xQueueReceive(void*q,void*p,unsigned t){return 0;}
''',
        'freertos/task.h': '''#pragma once
static inline int xTaskCreatePinnedToCore(void(*f)(void*),const char*n,unsigned s,void*a,unsigned p,void*h,int c){return 1;}
''',
        'driver/uart.h': '''#pragma once
#include <string.h>
#define UART_NUM_0 0
#define UART_NUM_1 1
#define GPIO_NUM_41 41
#define GPIO_NUM_42 42
#define GPIO_NUM_43 43
#define UART_DATA_8_BITS 8
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
#define UART_PIN_NO_CHANGE -1
typedef struct{int baud_rate,data_bits,parity,stop_bits,flow_ctrl,source_clk;}uart_config_t;
static char reply[2048];static int configured_port,configured_tx,configured_rx;
static inline int uart_write_bytes(int p,const void*s,unsigned n){memcpy(reply,s,n);reply[n]=0;return n;}
static inline int uart_read_bytes(int p,void*s,unsigned n,unsigned t){return 0;}
static inline int uart_param_config(int p,uart_config_t*c){configured_port=p;return c->baud_rate==921600?0:1;}
static inline int uart_set_pin(int p,int tx,int rx,int rts,int cts){configured_tx=tx;configured_rx=rx;return 0;}
static inline int uart_driver_install(int p,int rx,int tx,int n,void*q,int f){return 0;}
''',
        'nvs_flash.h': '#pragma once\nstatic inline int nvs_flash_init(void){return 0;}\n',
        'nvs.h': '''#pragma once
#include <stddef.h>
#include <string.h>
typedef int nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
static unsigned char saved[512];static size_t saved_size;
static inline int nvs_open(const char*n,int m,int*h){*h=1;return 0;}
static inline int nvs_get_blob(int h,const char*k,void*v,size_t*s){if(!saved_size)return 3;memcpy(v,saved,saved_size);*s=saved_size;return 0;}
static inline int nvs_set_blob(int h,const char*k,const void*v,size_t s){memcpy(saved,v,s);saved_size=s;return 0;}
static inline void nvs_close(int h){}
static inline int nvs_commit(int h){return 0;}
''',
    }
    for name_, text in stubs.items():
        p = folder / name_
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text)
    wrapper = f'#include "{(source / "scan_pose.c").as_posix()}"\n#undef TAG\n#include "{(source / "scan_uart.c").as_posix()}"\n'
    wrapper += r'''
__declspec(dllexport) int initialize(void){return scan_uart_init();}
__declspec(dllexport) void calibration(float*v){scan_camera_calibration_t c;scan_pose_get_calibration(&c);v[0]=c.width;v[1]=c.height;memcpy(v+2,&c.fx,9*sizeof(float));}
__declspec(dllexport) int set_cal(const float*v){scan_camera_calibration_t c={0};c.width=v[0];c.height=v[1];memcpy(&c.fx,v+2,9*sizeof(float));return scan_pose_set_calibration(&c);}
static XcrBarcodeResult barcode(const float*p,const char*payload,unsigned seq,int quality){
 XcrBarcodeResult r={0};r.geometry_valid=1;r.geometry_quality=quality;r.subpixel_valid=1;r.symbology=XCR_SYM_DATAMATRIX;
 r.sequence=seq;r.length=strlen(payload);memcpy(r.data,payload,r.length);memcpy(r.subpixel_corners,p,8*sizeof(float));return r;
}
__declspec(dllexport) int measure(const float*p,const char*payload,float*v){
 XcrBarcodeResult r=barcode(p,payload,1,2);scan_pose_result_t pose;
 if(!scan_pose_estimate(&r,&pose))return 0;
 v[0]=pose.yaw_y_deg;v[1]=pose.pitch_x_deg;v[2]=pose.roll_z_deg;v[3]=pose.range_m;
 v[4]=pose.optical_depth_m;v[5]=pose.distance_to_plane_m;v[6]=pose.tx_m;v[7]=pose.along_track_m;v[8]=pose.reprojection_px;return 1;
}
__declspec(dllexport) int emit(const float*p,const char*payload,unsigned seq,int quality,int full,unsigned long long deadline,char*out){
 atomic_store(&s_output_mode,full?OUTPUT_FULL:OUTPUT_COMPACT);
 XcrBarcodeResult r=barcode(p,payload,seq,quality);
 if(!scan_uart_submit(&r,deadline))return 0;
 const scan_uart_packet_t*packet=(const scan_uart_packet_t*)packet_bytes;
 memcpy(out,packet->line,packet->length);out[packet->length]=0;return packet->length;
}
__declspec(dllexport) void command(const char*cmd,char*out){
 reply[0]=0;char line[256];strcpy(line,cmd);
 if(!strncmp(cmd,"@OUT,",5))handle_output_command(cmd);else handle_calibration_command(line);
 strcpy(out,reply);
}
__declspec(dllexport) int wiring(void){return configured_port*10000+configured_tx*100+configured_rx;}
'''
    (folder / 'probe.c').write_text(wrapper)
    vs = subprocess.check_output(['C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe',
                                  '-latest', '-property', 'installationPath'], text=True).strip()
    batch = folder / 'compile.cmd'
    batch.write_text(f'@echo off\ncall "{vs}/VC/Auxiliary/Build/vcvars64.bat" >nul\n'
                     f'cl /nologo /O2 /std:c11 /LD /Dstrtok_r=strtok_s /I"{folder}" /I"{ROOT / "port/components/xcr_decoder/include"}" '
                     f'probe.c /link /OUT:probe.dll\n')
    run = subprocess.run(['cmd', '/c', str(batch)], cwd=folder, text=True, capture_output=True, errors='replace')
    if run.returncode:
        raise RuntimeError(run.stdout + run.stderr)
    lib = ctypes.CDLL(str(folder / 'probe.dll'))
    floats = ctypes.POINTER(ctypes.c_float)
    lib.calibration.argtypes = [floats]
    lib.set_cal.argtypes = [floats]
    lib.measure.argtypes = [floats, ctypes.c_char_p, floats]
    lib.emit.argtypes = [floats, ctypes.c_char_p, ctypes.c_uint, ctypes.c_int, ctypes.c_int,
                         ctypes.c_ulonglong, ctypes.c_void_p]
    lib.command.argtypes = [ctypes.c_char_p, ctypes.c_void_p]
    assert lib.initialize() == 0
    return lib


def main():
    current = compile_library('current', ROOT / 'main')
    reference = compile_library('reference', REFERENCE / 'port/standalone_idf_test/main')
    e6 = compile_library('e6', ROOT / 'main', 1024, 1024)
    floats = lambda a: np.ascontiguousarray(a, dtype=np.float32)
    ptr = lambda a: a.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
    defaults = []
    for lib in (reference, current, e6):
        c = np.zeros(11, np.float32)
        lib.calibration(ptr(c))
        defaults.append(c)
    assert np.array_equal(defaults[0], defaults[1]), defaults
    assert list(defaults[2][:2]) == [1024, 1024] and np.all(defaults[2][2:] == 0)
    c = defaults[0]
    matrix = np.array([[c[2], 0, c[4]], [0, c[3], c[5]], [0, 0, 1]], np.float64)
    distortion = np.array(c[6:], np.float64)
    cases, wire_cases = [], []
    for prefix, side in [('0', 30), ('1', 50), ('2', 70)]:
        half = side / 2
        obj = np.array([[-half, half, 0], [half, half, 0], [half, -half, 0], [-half, -half, 0]], np.float64)
        for yaw in (-30, -15, 0, 15, 30):
            for pitch in (-10, 0, 10):
                for distance in (350, 800, 1500):
                    y, p = np.deg2rad([yaw, pitch])
                    ry = np.array([[np.cos(y), 0, np.sin(y)], [0, 1, 0], [-np.sin(y), 0, np.cos(y)]])
                    rx = np.array([[1, 0, 0], [0, np.cos(p), -np.sin(p)], [0, np.sin(p), np.cos(p)]])
                    rotation, _ = cv2.Rodrigues(ry @ rx @ np.diag([1., -1., -1.]))
                    image, _ = cv2.projectPoints(obj, rotation, np.array([45., 10., distance]), matrix, distortion)
                    points = floats(image.reshape(8))
                    measurements = []
                    for lib in (reference, current):
                        values = np.zeros(9, np.float32)
                        assert lib.measure(ptr(points), (prefix + '12345').encode(), ptr(values)) == 1
                        measurements.append(values)
                    assert np.array_equal(*measurements), (prefix, yaw, pitch, distance, measurements)
                    cases.append(dict(prefix=prefix, yaw=yaw, pitch=pitch, distance_mm=distance))
                    for full in (0, 1):
                        outputs = []
                        for lib in (reference, current):
                            out = ctypes.create_string_buffer(2048)
                            assert lib.emit(ptr(points), (prefix + '12345').encode(), len(cases)+998, 2, full, 0, out) > 0
                            outputs.append(out.value)
                        assert outputs[0] == outputs[1]
                        import binascii
                        line, crc = outputs[0].strip().rsplit(b'*', 1)
                        assert binascii.crc_hqx(line, 0xffff) == int(crc, 16)
                        wire_cases.append(outputs[0].decode())
    for payload, quality, deadline in [(b'012345', 1, 0), (b'012345', 2, 999), (b'ABCD', 2, 0)]:
        for lib in (reference, current):
            assert lib.emit(ptr(points), payload, 0, quality, 0, deadline, ctypes.create_string_buffer(2048)) == 0
    replies = []
    for command in (b'@OUT,GET', b'@OUT,FULL', b'@OUT,GET', b'@OUT,COMPACT', b'@OUT,BAD', b'@CAL,GET',
                    b'@CAL,SET,1024,1024,800,800,511,511,0,0,0,0,0', b'@CAL,SET,1280,720,800,800,639,359,0,0,0,0,0', b'@CAL,GET'):
        outputs = []
        for lib in (reference, current):
            out = ctypes.create_string_buffer(2048)
            lib.command(command, out)
            outputs.append(out.value)
        assert outputs[0] == outputs[1], (command, outputs)
        replies.append(dict(command=command.decode(), reply=outputs[0].decode()))
    for lib in (reference, current):
        assert lib.initialize() == 0
        calibration = np.zeros(11, np.float32)
        lib.calibration(ptr(calibration))
        assert calibration[2] == 800, 'saved calibration did not override factory default'
    report = dict(passed=True, pose_cases=len(cases), byte_identical_uart_frames=len(wire_cases),
                  crc='All frames independently checked CRC16/CCITT-FALSE',
                  default_calibration=defaults[0].tolist(), e6_no_reference_intrinsics=True,
                  command_replies=replies, saved_calibration_priority=True,
                  current_wiring=current.wiring(), reference_wiring=reference.wiring(),
                  uart_examples=[wire_cases[0], wire_cases[1]],
                  current_source_sha256=hashlib.sha256((ROOT / 'main/scan_pose.c').read_bytes()).hexdigest(),
                  limitations='Platform/UART/NVS mocked; verifies actual C calculations and protocol bytes, not electrical UART or lens accuracy')
    (ROOT / 'docs/reference_pose_uart_validation.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
