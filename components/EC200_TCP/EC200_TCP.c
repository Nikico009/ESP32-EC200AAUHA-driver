#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "EC200_TCP.h"


/* -------------------------------------------------------------------------- */
/* MODEM AT COMMAND FUNCTIONS                                                 */
/* -------------------------------------------------------------------------- */

int mdm_send_cmd(const char *cmd, char *response, size_t response_size)
{
    char tx_buf[MAX_CMD];

    if (cmd == NULL)
        return EC200_ERROR;

    /* Append CRLF if the command does not already contain a line terminator. */
    if (strchr(cmd, '\r') == NULL && strchr(cmd, '\n') == NULL) {
        snprintf(tx_buf, sizeof(tx_buf), "%s\r\n", cmd);
    } else {
        snprintf(tx_buf, sizeof(tx_buf), "%s", cmd);
    }

    /* Remove stale UART data. */
    uart_flush_input(UART_PORT);

    /* Send command. */
    if (uart_write_bytes(UART_PORT, tx_buf, strlen(tx_buf)) < 0)
        return EC200_ERROR;

    /* No response requested. */
    if (response == NULL || response_size == 0)
        return EC200_OK;

    memset(response, 0, response_size);

    /* Wait for short modem response. */
    int len = uart_read_bytes(
        UART_PORT,
        (uint8_t *)response,
        response_size - 1,
        pdMS_TO_TICKS(1000)
    );

    if (len <= 0)
        return EC200_ERROR;

    response[len] = '\0';

    return EC200_OK;
}


int mdm_request_cmd(
    const char *cmd,
    char *response,
    size_t response_size,
    uint32_t timeout_ms
)
{
    char tx_buf[MAX_CMD];

    if (cmd == NULL || response == NULL || response_size == 0)
        return EC200_ERROR;

    /* Build command with CRLF. */
    if (strchr(cmd, '\r') == NULL && strchr(cmd, '\n') == NULL) {
        snprintf(tx_buf, sizeof(tx_buf), "%s\r\n", cmd);
    } else {
        snprintf(tx_buf, sizeof(tx_buf), "%s", cmd);
    }

    uart_flush_input(UART_PORT);

    if (uart_write_bytes(UART_PORT, tx_buf, strlen(tx_buf)) < 0)
        return EC200_ERROR;

    memset(response, 0, response_size);

    size_t total = 0;
    TickType_t start = xTaskGetTickCount();

    /* Collect the modem response until OK/ERROR or timeout. */
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {

        if (total >= response_size - 1)
            break;

        int len = uart_read_bytes(
            UART_PORT,
            (uint8_t *)&response[total],
            response_size - total - 1,
            pdMS_TO_TICKS(100)
        );

        if (len <= 0)
            continue;

        total += len;
        response[total] = '\0';

        /* Normal AT command completed successfully. */
        if (strstr(response, "\r\nOK\r\n") != NULL ||
            strstr(response, "\nOK\r\n") != NULL)
            return EC200_OK;

        /* Modem reported an error. */
        if (strstr(response, "\r\nERROR\r\n") != NULL ||
            strstr(response, "\nERROR\r\n") != NULL)
            return EC200_ERROR;
    }

    return EC200_ERROR;
}


/* -------------------------------------------------------------------------- */
/* MODEM INITIALIZATION                                                       */
/* -------------------------------------------------------------------------- */

int modem_init(void)
{
    char response[MAX_TCP_RESPONSE];
    uint8_t retries = EC200_RETRIES;

    /* Enable modem power. */
    gpio_set_level(EN_MDM, 0);
    gpio_set_level(PWR_KEY, 1);

    vTaskDelay(pdMS_TO_TICKS(5000));

    gpio_set_level(EN_MDM, 1);

    vTaskDelay(pdMS_TO_TICKS(150));

    /* Pulse PWR_KEY. */
    gpio_set_level(PWR_KEY, 0);

    vTaskDelay(pdMS_TO_TICKS(1800));

    gpio_set_level(PWR_KEY, 1);

    vTaskDelay(pdMS_TO_TICKS(5000));

    /* Wait until modem responds. */
    while (retries > 0) {

        if (mdm_send_cmd("AT", response, sizeof(response)) == EC200_OK) {

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


/* -------------------------------------------------------------------------- */
/* SIM CHECK                                                                  */
/* -------------------------------------------------------------------------- */

int modem_check_sim(void)
{
    char response[MAX_TCP_RESPONSE];

    if (mdm_send_cmd("AT+QSIMSTAT?", response, sizeof(response)) != EC200_OK)
        return EC200_ERROR;

    if (mdm_send_cmd("AT+QSIMDET=0,0", response, sizeof(response)) != EC200_OK)
        return EC200_ERROR;

    if (mdm_send_cmd("AT+CFUN=0", response, sizeof(response)) != EC200_OK)
        return EC200_ERROR;

    vTaskDelay(pdMS_TO_TICKS(1000));

    if (mdm_send_cmd("AT+CFUN=1", response, sizeof(response)) != EC200_OK)
        return EC200_ERROR;

    vTaskDelay(pdMS_TO_TICKS(3000));

    if (mdm_send_cmd("AT+CPIN?", response, sizeof(response)) != EC200_OK)
        return EC200_ERROR;

    if (strstr(response, "READY") == NULL)
        return EC200_ERROR;

    /* Query network information. */
    mdm_send_cmd("AT+CSQ", response, sizeof(response));
    mdm_send_cmd("AT+CEREG?", response, sizeof(response));
    mdm_send_cmd("AT+COPS?", response, sizeof(response));

    return EC200_OK;
}


/* -------------------------------------------------------------------------- */
/* TCP SOCKET                                                                 */
/* -------------------------------------------------------------------------- */

int tcp_open_socket(const char *host, int port, uint32_t timeout_ms)
{
    char response[MAX_TCP_RESPONSE];
    char cmd[MAX_CMD];

    if (host == NULL)
        return EC200_ERROR;

    /* Check LTE registration. */
    if (mdm_send_cmd("AT+CEREG?", response, sizeof(response)) != EC200_OK)
        return EC200_ERROR;

    /* Configure PDP context. */
    snprintf(
        cmd,
        sizeof(cmd),
        "AT+QICSGP=1,1,\"%s\",\"\",\"\",0",
        CLARO_APN
    );

    if (mdm_send_cmd(cmd, response, sizeof(response)) != EC200_OK)
        return EC200_ERROR;

    /* Activate PDP context. */
    if (mdm_send_cmd("AT+QIACT=1", response, sizeof(response)) != EC200_OK)
        return EC200_ERROR;

    /*
     * QIOPEN is asynchronous.
     * Therefore we cannot use mdm_send_cmd().
     */
    snprintf(
        cmd,
        sizeof(cmd),
        "AT+QIOPEN=1,0,\"TCP\",\"%s\",%d,0,1",
        host,
        port
    );

    uart_flush_input(UART_PORT);

    if (uart_write_bytes(UART_PORT, cmd, strlen(cmd)) < 0)
        return EC200_ERROR;

    if (uart_write_bytes(UART_PORT, "\r\n", 2) < 0)
        return EC200_ERROR;

    /* Wait for asynchronous QIOPEN result. */
    size_t total = 0;

    TickType_t start = xTaskGetTickCount();

    memset(response, 0, sizeof(response));

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {

        uint8_t buffer[64];

        int len = uart_read_bytes(
            UART_PORT,
            buffer,
            sizeof(buffer),
            pdMS_TO_TICKS(100)
        );

        if (len <= 0)
            continue;

        if (total < sizeof(response) - 1) {

            size_t copy_len = len;

            if (total + copy_len >= sizeof(response))
                copy_len = sizeof(response) - total - 1;

            memcpy(&response[total], buffer, copy_len);

            total += copy_len;

            response[total] = '\0';
        }

        /* Connection successful. */
        if (strstr(response, "+QIOPEN: 0,0") != NULL)
            return EC200_OK;

        /* Connection failed. */
        if (strstr(response, "+QIOPEN: 0,") != NULL)
            return EC200_ERROR;
    }

    return EC200_ERROR;
}


/* -------------------------------------------------------------------------- */
/* TCP SEND                                                                   */
/* -------------------------------------------------------------------------- */

int tcp_send_payload(
    const char *payload,
    size_t payload_length,
    uint32_t timeout_ms
)
{
    char cmd[MAX_CMD];
    char response[128];

    if (payload == NULL || payload_length == 0)
        return EC200_ERROR;

    /* Tell modem how many bytes will be sent. */
    snprintf(
        cmd,
        sizeof(cmd),
        "AT+QISEND=0,%zu",
        payload_length
    );

    uart_flush_input(UART_PORT);

    if (uart_write_bytes(UART_PORT, cmd, strlen(cmd)) < 0)
        return EC200_ERROR;

    if (uart_write_bytes(UART_PORT, "\r\n", 2) < 0)
        return EC200_ERROR;

    /* Wait for '>'. */
    TickType_t start = xTaskGetTickCount();

    bool ready = false;

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {

        uint8_t byte;

        int len = uart_read_bytes(
            UART_PORT,
            &byte,
            1,
            pdMS_TO_TICKS(100)
        );

        if (len > 0 && byte == '>') {
            ready = true;
            break;
        }
    }

    if (!ready)
        return EC200_ERROR;

    /* Send the complete payload. */
    if (uart_write_bytes(
            UART_PORT,
            payload,
            payload_length
        ) != payload_length)
        return EC200_ERROR;

    /*
     * Wait for SEND OK.
     *
     * We keep reading until SEND OK is found instead of doing
     * a single uart_read_bytes() call. This is important because
     * the modem response can arrive fragmented.
     */
    memset(response, 0, sizeof(response));

    size_t total = 0;

    start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {

        if (total >= sizeof(response) - 1)
            break;

        int len = uart_read_bytes(
            UART_PORT,
            (uint8_t *)&response[total],
            sizeof(response) - total - 1,
            pdMS_TO_TICKS(100)
        );

        if (len <= 0)
            continue;

        total += len;

        response[total] = '\0';

        if (strstr(response, "SEND OK") != NULL)
            return EC200_OK;

        if (strstr(response, "ERROR") != NULL)
            return EC200_ERROR;
    }

    return EC200_ERROR;
}


/* -------------------------------------------------------------------------- */
/* TCP RECEIVE                                                                */
/* -------------------------------------------------------------------------- */

int tcp_receive(
    void *buffer,
    size_t buffer_size,
    size_t *received,
    uint32_t timeout_ms
)
{
    uint8_t rx[256];

    size_t total = 0;

    TickType_t start = xTaskGetTickCount();
    TickType_t last_rx = start;

    if (buffer == NULL || buffer_size < 2 || received == NULL)
        return EC200_ERROR;

    *received = 0;

    /*
     * Receive everything that arrives during the timeout.
     *
     * The TCP data can arrive in multiple UART fragments, so
     * we continue reading after every fragment instead of
     * assuming that one UART read contains the whole HTTP response.
     */
    while (
        (xTaskGetTickCount() - start) <
        pdMS_TO_TICKS(timeout_ms)
    ) {

        int len = uart_read_bytes(
            UART_PORT,
            rx,
            sizeof(rx),
            pdMS_TO_TICKS(100)
        );

        if (len <= 0) {

            /*
             * Once data has arrived, consider the response complete
             * when no new bytes have arrived for 300 ms.
             */
            if (
                total > 0 &&
                (xTaskGetTickCount() - last_rx) >=
                pdMS_TO_TICKS(300)
            ) {
                break;
            }

            continue;
        }

        last_rx = xTaskGetTickCount();

        /*
         * Keep one byte reserved for the terminating '\0'.
         * This makes the received HTTP response directly printable
         * as a C string.
         */
        size_t available = buffer_size - total - 1;

        if (available == 0)
            break;

        size_t copy_len = (size_t)len;

        if (copy_len > available)
            copy_len = available;

        memcpy(
            (uint8_t *)buffer + total,
            rx,
            copy_len
        );

        total += copy_len;

        /*
         * Always keep the received data NULL-terminated.
         */
        ((uint8_t *)buffer)[total] = '\0';

        /*
         * If the application buffer is full, stop receiving.
         */
        if (total >= buffer_size - 1)
            break;
    }

    *received = total;

    if (total == 0)
        return EC200_NO_PAYLOAD;

    return EC200_OK;
}


/* -------------------------------------------------------------------------- */
/* TCP CLOSE                                                                  */
/* -------------------------------------------------------------------------- */

int tcp_close_socket(void)
{
    char response[128];

    return mdm_send_cmd(
        "AT+QICLOSE=0",
        response,
        sizeof(response)
    );
}