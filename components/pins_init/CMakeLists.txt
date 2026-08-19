idf_component_register(
    SRCS "pins_init.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_gpio esp_driver_uart ec200
)