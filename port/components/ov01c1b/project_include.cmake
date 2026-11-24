if(CONFIG_CAMERA_OV01C1B)
    idf_build_set_property(ESP_IPA_JSON_CONFIG_FILE_PATH
                           "${COMPONENT_PATH}/ov01c1b_ipa.json" APPEND)
endif()
