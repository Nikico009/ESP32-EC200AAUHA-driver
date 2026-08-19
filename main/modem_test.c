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

    // Modem state
    modem_state_t modem_state = {
        .modem_communication = false,
        .sim_detect = false
    };

    // Initialize hardware
    printf("Initializing hardware...\n");
    pins_init();

    // Initialize modem
    printf("Powering on and initializing modem...\n");
    modem_state.modem_communication =
        (modem_init() == EC200_OK);

    if (modem_state.modem_communication) {

        gpio_set_level(LED_AUX, 1);

        printf("UART communication established. Checking SIM...\n");

        modem_state.sim_detect =
            (modem_check_sim() == EC200_OK);

        if (modem_state.sim_detect) {
            printf("SIM detected and ready.\n");
        } else {
            printf("SIM not detected or requires a PIN.\n");
        }

    } else {

        gpio_set_level(LED_AUX, 0);

        printf(
            "Error: Modem did not respond after %d retries.\n",
            EC200_RETRIES
        );
    }

    // Open TCP socket to httpbin.org on port 80
    printf("Opening TCP socket to httpbin.org:80...\n");

    if (open_tcp_socket("httpbin.org", 80) == EC200_OK) {
        printf("TCP socket opened successfully.\n");
    } else {
        printf("Error opening TCP socket.\n");
    }

    // Blink status LED
    while (true) {

        led_state = !led_state;
        gpio_set_level(LED_STA, led_state);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}