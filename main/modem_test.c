#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "ec200.h"
#include "pins_init.h"

void app_main(void)
{
    bool led_state = false;


    // Modem descriptor
    modem_state_t modem_state = {
        .modem_communication = false,
        .sim_detect = false
    };

    printf("Iniciando hardware...\n");
    pins_init();

    printf("Encendiendo e inicializando modem...\n");
    modem_state.modem_communication = modem_init();

    if (modem_state.modem_communication) {
        gpio_set_level(LED_AUX, 1);
        printf("Comunicacion UART exitosa. Verificando SIM...\n");
        modem_state.sim_detect = modem_check_sim();
        if (modem_state.sim_detect) {
            printf("SIM detectada y registrada en red.\n");
        } else {
            printf("SIM no detectada o requiere PIN.\n");
        }
    } else {
        gpio_set_level(LED_AUX, 0);
        printf(
            "Error: No hubo respuesta del modem tras %d reintentos.\n",
            RETRIES
        );
    }

    while (true) {

        led_state = !led_state;
        gpio_set_level(LED_STA, led_state);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}