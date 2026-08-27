#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "EC200_TCP.h"


/* -------------------------------------------------------------------------- */
/* INTERNAL STATE                                                             */
/* -------------------------------------------------------------------------- */

static char at_response[MAX_AT_RESPONSE];
static char socket_response[MAX_AT_RESPONSE];
static char cmd_buffer[MAX_CMD];

static const char *current_apn = NULL;


/* -------------------------------------------------------------------------- */
/* MODEM AT COMMAND FUNCTIONS                                                 */
/* -------------------------------------------------------------------------- */

int mdm_send_cmd(const char *cmd, char *response, size_t response_size)
{
    size_t len;

    if (cmd == NULL)
        return EC200_ERROR;

    if (strchr(cmd, '\r') == NULL && strchr(cmd, '\n') == NULL)
        len = snprintf(cmd_buffer, sizeof(cmd_buffer), "%s\r\n", cmd);
    else
        len = snprintf(cmd_buffer, sizeof(cmd_buffer), "%s", cmd);

    if (len >= sizeof(cmd_buffer))
        return EC200_ERROR;

    uart_flush_input(UART_PORT);

    if (uart_write_bytes(UART_PORT, cmd_buffer, len) < 0)
        return EC200_ERROR;

    if (response == NULL || response_size == 0)
        return EC200_OK;

    memset(response, 0, response_size);

    int rx_len = uart_read_bytes(
        UART_PORT,
        (uint8_t *)response,
        response_size - 1,
        pdMS_TO_TICKS(1000)
    );

    if (rx_len <= 0)
        return EC200_ERROR;

    response[rx_len] = '\0';

    return EC200_OK;
}


int mdm_request_cmd(const char *cmd, char *response, size_t response_size, uint32_t timeout_ms)
{
    size_t total = 0;
    TickType_t start;

    if (cmd == NULL || response == NULL || response_size == 0)
        return EC200_ERROR;

    if (strchr(cmd, '\r') == NULL && strchr(cmd, '\n') == NULL)
        snprintf(cmd_buffer, sizeof(cmd_buffer), "%s\r\n", cmd);
    else
        snprintf(cmd_buffer, sizeof(cmd_buffer), "%s", cmd);

    if (strlen(cmd_buffer) >= sizeof(cmd_buffer))
        return EC200_ERROR;

    uart_flush_input(UART_PORT);

    if (uart_write_bytes(UART_PORT, cmd_buffer, strlen(cmd_buffer)) < 0)
        return EC200_ERROR;

    memset(response, 0, response_size);

    start = xTaskGetTickCount();

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

        if (strstr(response, "\r\nOK\r\n") != NULL ||
            strstr(response, "\nOK\r\n") != NULL)
            return EC200_OK;

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
    uint8_t retries = EC200_RETRIES;

    gpio_set_level(EN_MDM, 0);
    gpio_set_level(PWR_KEY, 1);

    vTaskDelay(pdMS_TO_TICKS(5000));

    gpio_set_level(EN_MDM, 1);

    vTaskDelay(pdMS_TO_TICKS(150));

    gpio_set_level(PWR_KEY, 0);

    vTaskDelay(pdMS_TO_TICKS(1800));

    gpio_set_level(PWR_KEY, 1);

    vTaskDelay(pdMS_TO_TICKS(5000));

    while (retries > 0) {

        if (mdm_send_cmd("AT", at_response, sizeof(at_response)) == EC200_OK) {

            if (strstr(at_response, "OK") != NULL) {
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
    if (mdm_send_cmd("AT+QSIMSTAT?", at_response, sizeof(at_response)) != EC200_OK)
        return EC200_ERROR;

    if (mdm_send_cmd("AT+QSIMDET=0,0", at_response, sizeof(at_response)) != EC200_OK)
        return EC200_ERROR;

    if (mdm_send_cmd("AT+CFUN=0", at_response, sizeof(at_response)) != EC200_OK)
        return EC200_ERROR;

    vTaskDelay(pdMS_TO_TICKS(1000));

    if (mdm_send_cmd("AT+CFUN=1", at_response, sizeof(at_response)) != EC200_OK)
        return EC200_ERROR;

    vTaskDelay(pdMS_TO_TICKS(3000));

    if (mdm_send_cmd("AT+CPIN?", at_response, sizeof(at_response)) != EC200_OK)
        return EC200_ERROR;

    if (strstr(at_response, "READY") == NULL)
        return EC200_ERROR;

    if (mdm_send_cmd("AT+COPS?", at_response, sizeof(at_response)) != EC200_OK)
        return EC200_ERROR;

    if (strstr(at_response, "CLARO") != NULL ||
        strstr(at_response, "Claro") != NULL ||
        strstr(at_response, "claro") != NULL) {

        current_apn = CLARO_APN;

    } else if (strstr(at_response, "MOVISTAR") != NULL ||
               strstr(at_response, "Movistar") != NULL ||
               strstr(at_response, "movistar") != NULL) {

        current_apn = MOVISTAR_APN;

    } else if (strstr(at_response, "PERSONAL") != NULL ||
               strstr(at_response, "Personal") != NULL ||
               strstr(at_response, "personal") != NULL) {

        current_apn = PERSONAL_APN;

    } else {
        current_apn = NULL;
        return EC200_ERROR;
    }

    mdm_send_cmd("AT+CSQ", at_response, sizeof(at_response));
    mdm_send_cmd("AT+CEREG?", at_response, sizeof(at_response));

    return EC200_OK;
}


/* -------------------------------------------------------------------------- */
/* TCP SOCKET                                                                 */
/* -------------------------------------------------------------------------- */

int tcp_open_socket(const char *host, int port, uint32_t timeout_ms)
{
    size_t total = 0;
    TickType_t start;

    if (host == NULL || current_apn == NULL)
        return EC200_ERROR;

    if (mdm_send_cmd("AT+CEREG?", at_response, sizeof(at_response)) != EC200_OK)
        return EC200_ERROR;

    snprintf(
        cmd_buffer,
        sizeof(cmd_buffer),
        "AT+QICSGP=1,1,\"%s\",\"\",\"\",0",
        current_apn
    );

    if (strlen(cmd_buffer) >= sizeof(cmd_buffer))
        return EC200_ERROR;

    if (mdm_send_cmd(cmd_buffer, at_response, sizeof(at_response)) != EC200_OK)
        return EC200_ERROR;

    if (mdm_send_cmd("AT+QIACT=1", at_response, sizeof(at_response)) != EC200_OK)
        return EC200_ERROR;

    snprintf(
        cmd_buffer,
        sizeof(cmd_buffer),
        "AT+QIOPEN=1,0,\"TCP\",\"%s\",%d,0,1",
        host,
        port
    );

    if (strlen(cmd_buffer) >= sizeof(cmd_buffer))
        return EC200_ERROR;

    uart_flush_input(UART_PORT);

    if (uart_write_bytes(UART_PORT, cmd_buffer, strlen(cmd_buffer)) < 0)
        return EC200_ERROR;

    if (uart_write_bytes(UART_PORT, "\r\n", 2) < 0)
        return EC200_ERROR;

    memset(socket_response, 0, sizeof(socket_response));

    start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {

        uint8_t buffer[32];

        int len = uart_read_bytes(
            UART_PORT,
            buffer,
            sizeof(buffer),
            pdMS_TO_TICKS(100)
        );

        if (len <= 0)
            continue;

        if (total < sizeof(socket_response) - 1) {

            size_t copy_len = len;

            if (total + copy_len >= sizeof(socket_response))
                copy_len = sizeof(socket_response) - total - 1;

            memcpy(&socket_response[total], buffer, copy_len);

            total += copy_len;

            socket_response[total] = '\0';
        }

        if (strstr(socket_response, "+QIOPEN: 0,0") != NULL)
            return EC200_OK;

        if (strstr(socket_response, "+QIOPEN: 0,") != NULL)
            return EC200_ERROR;
    }

    return EC200_ERROR;
}


/* -------------------------------------------------------------------------- */
/* TCP SEND                                                                   */
/* -------------------------------------------------------------------------- */

int tcp_send_payload(const char *payload, size_t payload_length, uint32_t timeout_ms)
{
    size_t total = 0;
    TickType_t start;

    if (payload == NULL || payload_length == 0)
        return EC200_ERROR;

    snprintf(
        cmd_buffer,
        sizeof(cmd_buffer),
        "AT+QISEND=0,%zu",
        payload_length
    );

    if (strlen(cmd_buffer) >= sizeof(cmd_buffer))
        return EC200_ERROR;

    uart_flush_input(UART_PORT);

    if (uart_write_bytes(UART_PORT, cmd_buffer, strlen(cmd_buffer)) < 0)
        return EC200_ERROR;

    if (uart_write_bytes(UART_PORT, "\r\n", 2) < 0)
        return EC200_ERROR;

    start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {

        uint8_t byte;

        if (uart_read_bytes(
                UART_PORT,
                &byte,
                1,
                pdMS_TO_TICKS(100)
            ) > 0) {

            if (byte == '>')
                break;
        }
    }

    if (xTaskGetTickCount() - start >= pdMS_TO_TICKS(timeout_ms))
        return EC200_ERROR;

    if (uart_write_bytes(UART_PORT, payload, payload_length) != payload_length)
        return EC200_ERROR;

    memset(at_response, 0, sizeof(at_response));

    start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {

        if (total >= sizeof(at_response) - 1)
            break;

        int len = uart_read_bytes(
            UART_PORT,
            (uint8_t *)&at_response[total],
            sizeof(at_response) - total - 1,
            pdMS_TO_TICKS(100)
        );

        if (len <= 0)
            continue;

        total += len;
        at_response[total] = '\0';

        if (strstr(at_response, "SEND OK") != NULL)
            return EC200_OK;

        if (strstr(at_response, "ERROR") != NULL)
            return EC200_ERROR;
    }

    return EC200_ERROR;
}


/* -------------------------------------------------------------------------- */
/* TCP RECEIVE                                                                */
/* -------------------------------------------------------------------------- */

int tcp_receive(void *buffer, size_t buffer_size, size_t *received, uint32_t timeout_ms)
{
    uint8_t rx[32];
    size_t total = 0;
    TickType_t start = xTaskGetTickCount();
    TickType_t last_rx = start;

    if (buffer == NULL || buffer_size < 2 || received == NULL)
        return EC200_ERROR;

    *received = 0;

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {

        int len = uart_read_bytes(
            UART_PORT,
            rx,
            sizeof(rx),
            pdMS_TO_TICKS(100)
        );

        if (len <= 0) {

            if (total > 0 &&
                (xTaskGetTickCount() - last_rx) >= pdMS_TO_TICKS(300))
                break;

            continue;
        }

        last_rx = xTaskGetTickCount();

        size_t available = buffer_size - total - 1;

        if (available == 0)
            break;

        size_t copy_len = (size_t)len;

        if (copy_len > available)
            copy_len = available;

        memcpy((uint8_t *)buffer + total, rx, copy_len);

        total += copy_len;

        ((uint8_t *)buffer)[total] = '\0';
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
    return mdm_send_cmd(
        "AT+QICLOSE=0",
        at_response,
        sizeof(at_response)
    );
}