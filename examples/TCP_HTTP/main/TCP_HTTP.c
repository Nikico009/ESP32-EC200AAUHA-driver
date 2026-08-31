#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "EC200_MODEM.h"
#include "EC200_TCP.h"
#include "pins_init.h"


#define TCP_OPEN_TIMEOUT_MS       10000
#define TCP_SEND_TIMEOUT_MS        5000
#define TCP_RECEIVE_TIMEOUT_MS     5000


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

        printf(
            "Error: Modem did not respond after %d retries.\n",
            EC200_RETRIES
        );

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


    printf("Opening TCP socket to httpbin.org:80...\n");

    if (tcp_open_socket(
            "httpbin.org",
            80,
            TCP_OPEN_TIMEOUT_MS) != EC200_OK) {

        printf("Error opening TCP socket.\n");
        return;
    }

    printf("TCP socket opened successfully.\n");


    const char payload[] =
        "GET /get HTTP/1.1\r\n"
        "Host: httpbin.org\r\n"
        "Connection: close\r\n"
        "\r\n";


    if (tcp_send_payload(
            payload,
            strlen(payload),
            TCP_SEND_TIMEOUT_MS) != EC200_OK) {

        printf("Failed sending HTTP request.\n");
        tcp_close_socket();
        return;
    }

    printf("HTTP request sent correctly.\n");


    static uint8_t response[MAX_TCP_RESPONSE];
    size_t received = 0;

    int result = tcp_receive(
        response,
        sizeof(response) - 1,
        &received,
        TCP_RECEIVE_TIMEOUT_MS
    );

    if (result == EC200_OK) {

        response[received] = '\0';

        printf(
            "\n================ HTTP RESPONSE ================\n"
        );

        printf("%s", response);

        printf(
            "\n================================================\n"
        );

        printf(
            "Received %zu bytes.\n",
            received
        );

    } else if (result == EC200_NO_PAYLOAD) {

        printf("No TCP data available.\n");

    } else {

        printf("Failed receiving HTTP response.\n");
    }


    if (tcp_close_socket() == EC200_OK)
        printf("TCP socket closed successfully.\n");
    else
        printf("Failed closing TCP socket.\n");


    while (1) {
        led_state = !led_state;
        gpio_set_level(LED_STA, led_state);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}