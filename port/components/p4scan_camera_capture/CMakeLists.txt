idf_component_register(
    SRCS "p4scan_camera_capture.c"
    INCLUDE_DIRS "include"
    REQUIRES p4scan_sensor_profile esp_video esp_driver_i2c esp_driver_gpio esp_mm esp_timer xcr_decoder ov01c1b p4scan_board_power
)
