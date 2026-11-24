if(P4SCAN_REPLAY_BUNDLE)
    set(replay_bundle "${CMAKE_CURRENT_BINARY_DIR}/p4scan_replay.bin")
    configure_file("${P4SCAN_REPLAY_BUNDLE}" "${replay_bundle}" COPYONLY)
    idf_component_register(
        SRCS "../../../tools/p4scan_replay.c"
        PRIV_INCLUDE_DIRS "../port/components/xcr_decoder/vendor/include"
        REQUIRES xcr_decoder esp_driver_jpeg esp_timer
        EMBED_FILES "${replay_bundle}")
    return()
endif()

if(CONFIG_P4SCAN_SENSOR_OV9281 AND (NOT CONFIG_CAMERA_OV9281 OR CONFIG_CAMERA_OV01C1B))
    message(FATAL_ERROR "OV9281 profile requires only CAMERA_OV9281; use sdkconfig.ov9281")
endif()
if(CONFIG_P4SCAN_SENSOR_E6 AND (NOT CONFIG_CAMERA_OV01C1B OR CONFIG_CAMERA_OV9281))
    message(FATAL_ERROR "E6 profile requires only CAMERA_OV01C1B; use sdkconfig.e6")
endif()

idf_component_register(
    SRCS "board_io.c" "scan_uart.c" "scan_pose.c" "camera_main.c" "scan_log_buffer.c" "scan_usb.c" "agv_pipeline.c"
    PRIV_INCLUDE_DIRS "../port/components/xcr_decoder/vendor/include"
                      "../port/components/xcr_decoder/vendor/quirc"
    REQUIRES p4scan_camera_capture p4scan_board_power p4scan_feedback
             xcr_decoder esp_timer esp_psram esp_partition esp_driver_gpio esp_driver_uart nvs_flash
             usb_device_uvc espressif__tinyusb esp_driver_jpeg)
