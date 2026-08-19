#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "ec200.h"
#include "pins_init.h"


#define TCP_OPEN_TIMEOUT_S     10
#define TCP_PAYLOAD_TIMEOUT_S  5
#define TCP_RECEIVE_TIMEOUT_S  5


void app_main(void)
{
    bool led_state = false;

    modem_state_t modem_state = {
        .modem_communication = 0,
        .sim_detect = 0,
        .tcp_socket = 0
    };

    printf("Initializing hardware...\n");
    pins_init();

    /* ------------------------------ MODEM INITI ------------------------------ */

    printf("Powering on and initializing modem...\n");

    modem_state.modem_communication = modem_init() == EC200_OK;

    if (!modem_state.modem_communication) {
        gpio_set_level(LED_AUX, 0);
        printf("Error: Modem did not respond after %d retries.\n", EC200_RETRIES);
        return;
    }

    gpio_set_level(LED_AUX, 1);
    printf("UART communication established. Checking SIM...\n");

    /* ------------------------------ SIM DETECTION ------------------------------ */

    modem_state.sim_detect = modem_check_sim() == EC200_OK;

    if (!modem_state.sim_detect) {
        printf("SIM not detected or requires a PIN.\n");
        return;
    }

    printf("SIM detected and ready.\n");

    /* ------------------------------ TCP ------------------------------ */

    printf("Opening TCP socket to httpbin.org:80...\n");

    modem_state.tcp_socket = tcp_open_socket("httpbin.org", 80, TCP_OPEN_TIMEOUT_S * 1000);

    if (modem_state.tcp_socket != EC200_OK) {
        printf("Error opening TCP socket.\n");
        return;
    }

    modem_state.tcp_socket = true;
    printf("TCP socket opened successfully.\n");

    /* ------------------------------ HTTP PAYLOAD ----------------------------- */

    const char payload[] =
        "GET /get HTTP/1.1\r\n"
        "Host: httpbin.org\r\n"
        "Connection: close\r\n"
        "\r\n";

    if (tcp_send_payload(payload, strlen(payload), TCP_PAYLOAD_TIMEOUT_S * 1000) != EC200_OK) {
        printf("Failed sending payload.\n");
        tcp_close_socket();
        return;
    }

    printf("HTTP request sent correctly.\n");

    /* --------------------------- TCP RECEIVE ------------------------- */

    uint8_t response[MAX_TCP_RESPONSE];
    size_t received = 0;

    int result = tcp_receive(
        response,
        sizeof(response),
        &received,
        TCP_RECEIVE_TIMEOUT_S * 1000
    );

    if (result == EC200_OK) {
        printf("Received %zu bytes:\n", received);

        /*
         * HTTP response is text, so we can temporarily
         * terminate it for printing.
         */
        size_t printable = received;

        if (printable >= sizeof(response))
            printable = sizeof(response) - 1;

        response[printable] = '\0';

        printf("%s\n", response);

    } else if (result == EC200_NO_PAYLOAD) {
        printf("No TCP data available.\n");
    } else {
        printf("Failed receiving TCP response.\n");
    }

    /* ----------------------------- CLOSE ----------------------------- */

    if (tcp_close_socket() == EC200_OK) {
        modem_state.tcp_socket = false;
        printf("TCP socket closed successfully.\n");
    } else {
        printf("Failed closing TCP socket.\n");
    }

    /* ------------------------------ LED ------------------------------ */

    while (true) {
        led_state = !led_state;
        gpio_set_level(LED_STA, led_state);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}