idf_component_register(SRCS "src/orp_sensor_driver.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_adc nvs_flash)
