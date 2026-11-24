set(sensor_sources)
if(CONFIG_CAMERA_OV01C1B)
    list(APPEND sensor_sources "ov01c1b.c")
endif()
idf_component_register(
    SRCS ${sensor_sources}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_include"
    REQUIRES esp_cam_sensor esp_sccb_intf esp_driver_gpio
)

if(CONFIG_CAMERA_OV01C1B)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ov01c1b_detect")
endif()
