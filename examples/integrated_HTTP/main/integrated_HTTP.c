#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "EC200_MODEM.h"
#include "EC200_integrated_HTTP.h"
#include "pins_init.h"


void app_main(void)
{
    bool led_state = false;


    printf("Initializing hardware...\n");
    pins_init();


    printf("Initializing modem UART...\n");

    if (ec200_uart_init() != EC200_OK) {
        printf("Error initializing UART.\n");
        return;
    }

    printf("UART communication established.\n");


    printf("Powering on and initializing modem...\n");

    if (modem_init() != EC200_OK) {
        gpio_set_level(LED_AUX, 0);
        printf("Error: Modem did not respond after %d retries.\n", EC200_RETRIES);
        return;
    }

    gpio_set_level(LED_AUX, 1);

    printf("Modem initialized successfully.\n");


    printf("Checking SIM...\n");

    if (modem_check_sim() != EC200_OK) {
        printf("SIM not detected or requires a PIN.\n");
        return;
    }

    printf("SIM detected and ready.\n");

    while(true){
        led_state = !led_state;
        gpio_set_level(LED_STA, led_state);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

}
