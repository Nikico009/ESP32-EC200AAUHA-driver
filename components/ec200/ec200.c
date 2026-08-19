#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "ec200.h"


int send_mdm(const char *cmd, char *response, size_t response_size)
{
    char tx_buf[128];

    /* Validate command pointer */
    if (cmd == NULL) {
        return EC200_ERROR;
    }

    /*
     * Append CRLF if the command does not already
     * contain a line terminator.
     */
    if (strchr(cmd, '\r') == NULL && strchr(cmd, '\n') == NULL) {
        snprintf(tx_buf, sizeof(tx_buf), "%s\r\n", cmd);
    } else {
        snprintf(tx_buf, sizeof(tx_buf), "%s", cmd);
    }

    /* Remove stale data from the UART receive buffer */
    uart_flush_input(UART_PORT);

    /* Send command to the modem */
    if (uart_write_bytes(UART_PORT, tx_buf, strlen(tx_buf)) < 0) {
        return EC200_ERROR;
    }

    /*
     * If no response buffer was provided, consider the
     * transmission successful without waiting for data.
     */
    if (response == NULL || response_size == 0) {
        return EC200_OK;
    }

    memset(response, 0, response_size);

    /* Wait for modem response */
    int len = uart_read_bytes(
        UART_PORT,
        (uint8_t *)response,
        response_size - 1,
        pdMS_TO_TICKS(1000)
    );

    if (len <= 0) {
        return EC200_ERROR;
    }

    response[len] = '\0';

    return EC200_OK;
}


int modem_init(void)
{
    char response[64];
    uint8_t retries = EC200_RETRIES;

    /* Enable modem power */
    gpio_set_level(EN_MDM, 0);
    gpio_set_level(PWR_KEY, 1);

    vTaskDelay(pdMS_TO_TICKS(5000));

    gpio_set_level(EN_MDM, 1);

    vTaskDelay(pdMS_TO_TICKS(150));

    /* Pulse PWR_KEY to start the modem */
    gpio_set_level(PWR_KEY, 0);
    vTaskDelay(pdMS_TO_TICKS(1800));

    gpio_set_level(PWR_KEY, 1);
    vTaskDelay(pdMS_TO_TICKS(5000));

    /* Wait until the modem responds to AT commands */
    while (retries > 0) {

        if (send_mdm("AT", response, sizeof(response)) == EC200_OK) {

            if (strstr(response, "OK") != NULL) {
                gpio_set_level(LED_STA, 1);
                return EC200_OK;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
        retries--;
    }

    return EC200_ERROR;
}


int modem_check_sim(void)
{
    char response[256];

    /* Configure SIM detection */
    if (send_mdm("AT+QSIMSTAT?", response, sizeof(response)) != EC200_OK) {
        return EC200_ERROR;
    }

    if (send_mdm("AT+QSIMDET=0,0", response, sizeof(response)) != EC200_OK) {
        return EC200_ERROR;
    }

    /* Restart the modem functionality */
    if (send_mdm("AT+CFUN=0", response, sizeof(response)) != EC200_OK) {
        return EC200_ERROR;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    if (send_mdm("AT+CFUN=1", response, sizeof(response)) != EC200_OK) {
        return EC200_ERROR;
    }

    vTaskDelay(pdMS_TO_TICKS(3000));

    /* Check SIM status */
    if (send_mdm("AT+CPIN?", response, sizeof(response)) != EC200_OK) {
        return EC200_ERROR;
    }

    if (strstr(response, "READY") == NULL) {
        return EC200_ERROR;
    }

    /* Query signal and network information */
    send_mdm("AT+CSQ", response, sizeof(response));
    send_mdm("AT+CEREG?", response, sizeof(response));
    send_mdm("AT+COPS?", response, sizeof(response));

    return EC200_OK;
}


int open_tcp_socket(const char *host, int port)
{
    char response[128];
    char cmd[128];

    if (host == NULL) {
        return EC200_ERROR;
    }

    /* Check LTE registration status */
    if (send_mdm("AT+CEREG?", response, sizeof(response)) != EC200_OK) {
        return EC200_ERROR;
    }

    /* Configure PDP context with the selected APN */
    snprintf(
        cmd,
        sizeof(cmd),
        "AT+QICSGP=1,1,\"%s\",\"\",\"\",0",
        CLARO_APN
    );

    if (send_mdm(cmd, response, sizeof(response)) != EC200_OK) {
        return EC200_ERROR;
    }

    /* Activate PDP context */
    if (send_mdm("AT+QIACT=1", response, sizeof(response)) != EC200_OK) {
        return EC200_ERROR;
    }

    /* Open TCP socket */
    snprintf(
        cmd,
        sizeof(cmd),
        "AT+QIOPEN=1,0,\"TCP\",\"%s\",%d,0,1",
        host,
        port
    );

    if (send_mdm(cmd, response, sizeof(response)) != EC200_OK) {
        return EC200_ERROR;
    }

    return EC200_OK;
}