idf_component_register(
    SRCS "ec200.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_uart esp_driver_gpio freertos pins_init
)