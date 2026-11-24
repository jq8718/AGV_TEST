idf_component_register(SRCS "main.c" "aw36515.c" "stability.c" INCLUDE_DIRS "."
                      REQUIRES esp_driver_gpio esp_driver_i2c esp_psram spi_flash esp_hw_support esp_system heap esp_partition app_update)
