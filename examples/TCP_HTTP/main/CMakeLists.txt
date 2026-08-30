idf_component_register(SRCS "TCP_HTTP.c"
                    INCLUDE_DIRS "."
                    REQUIRES EC200_TCP pins_init EC200_MODEM)
