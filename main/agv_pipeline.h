#pragma once
#include "p4scan_camera_capture.h"

esp_err_t agv_pipeline_start(p4scan_camera_t *camera);

void agv_light_request(int request); /* 0..3: illumination bit0, aiming bit1; 4: auto */
void agv_board_status_request(void);
