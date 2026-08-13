idf_component_register(SRCS "modem_test.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_driver_uart esp_driver_gpio esp_timer ec200 pins_init)
