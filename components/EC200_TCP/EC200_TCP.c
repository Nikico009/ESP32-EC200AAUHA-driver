#include "EC200_TCP.h"

#include "EC200_MODEM.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define TCP_QIRD_MAX_LENGTH 1024
#define TCP_QIRD_OVERHEAD 64


/**
 * @brief Wait for a specific string from the modem.
 */
static int mdm_wait_response(
    const char *expected,
    char *response,
    size_t response_size,
    uint32_t timeout_ms
)
{
    size_t total_received = 0;
    size_t received = 0;
    uint32_t elapsed = 0;

    if (expected == NULL ||
        response == NULL ||
        response_size < 2)
        return EC200_ERROR;

    response[0] = '\0';

    while (elapsed < timeout_ms) {

        if (total_received >= response_size - 1)
            break;

        int result = ec200_uart_read(
            response + total_received,
            response_size - 1 - total_received,
            &received,
            100
        );

        if (result == EC200_OK) {

            total_received += received;
            response[total_received] = '\0';

            if (strstr(response, expected) != NULL)
                return EC200_OK;

            if (strstr(response, "\r\nERROR\r\n") != NULL)
                return EC200_ERROR;

            if (strstr(response, "+CME ERROR:") != NULL)
                return EC200_ERROR;
        }

        elapsed += 100;
    }

    response[total_received] = '\0';

    return EC200_ERROR;
}


/**
 * @brief Send an AT command and wait for OK.
 */
static int mdm_send_command(
    const char *command,
    char *response,
    size_t response_size,
    uint32_t timeout_ms
)
{
    if (command == NULL)
        return EC200_ERROR;

    if (ec200_uart_write(
            command,
            strlen(command)) != EC200_OK)
        return EC200_ERROR;

    return mdm_wait_response(
        "\r\nOK\r\n",
        response,
        response_size,
        timeout_ms
    );
}


int tcp_open_socket(
    const char *host,
    int port,
    uint32_t timeout_ms
)
{
    char command[128];
    static char response[MAX_TCP_RESPONSE];

    if (host == NULL)
        return EC200_ERROR;

    ec200_uart_flush();

    snprintf(
        command,
        sizeof(command),
        "AT+QIOPEN=1,0,\"TCP\",\"%s\",%d,0,0\r\n",
        host,
        port
    );

    if (ec200_uart_write(
            command,
            strlen(command)) != EC200_OK)
        return EC200_ERROR;

    /*
     * QIOPEN first returns OK and then
     * asynchronously returns +QIOPEN.
     */
    if (mdm_wait_response(
            "+QIOPEN:",
            response,
            sizeof(response),
            timeout_ms) != EC200_OK)
        return EC200_ERROR;

    if (strstr(
            response,
            "+QIOPEN: 0,0") == NULL)
        return EC200_ERROR;

    return EC200_OK;
}


int tcp_send_payload(
    const char *payload,
    size_t payload_length,
    uint32_t timeout_ms
)
{
    char command[64];
    char response[128];

    if (payload == NULL ||
        payload_length == 0)
        return EC200_ERROR;

    snprintf(
        command,
        sizeof(command),
        "AT+QISEND=0,%zu\r\n",
        payload_length
    );

    /*
     * Request the modem to enter
     * transmission mode.
     */
    if (ec200_uart_write(
            command,
            strlen(command)) != EC200_OK)
        return EC200_ERROR;

    /*
     * Wait for the '>' prompt.
     */
    if (mdm_wait_response(
            ">",
            response,
            sizeof(response),
            timeout_ms) != EC200_OK)
        return EC200_ERROR;

    /*
     * Send the exact payload length.
     */
    if (ec200_uart_write(
            payload,
            payload_length) != EC200_OK)
        return EC200_ERROR;

    /*
     * Wait for SEND OK.
     */
    if (mdm_wait_response(
            "SEND OK",
            response,
            sizeof(response),
            timeout_ms) != EC200_OK)
        return EC200_ERROR;

    return EC200_OK;
}


int tcp_receive(
    void *buffer,
    size_t buffer_size,
    size_t *received,
    uint32_t timeout_ms
)
{
    char command[32];
    static char response[TCP_QIRD_MAX_LENGTH + TCP_QIRD_OVERHEAD];

    size_t total_received = 0;
    size_t chunk_received = 0;
    uint32_t elapsed = 0;
    size_t qird_length;

    if (buffer == NULL ||
        received == NULL ||
        buffer_size == 0)
        return EC200_ERROR;

    *received = 0;

    qird_length = buffer_size;

    if (qird_length > TCP_QIRD_MAX_LENGTH)
        qird_length = TCP_QIRD_MAX_LENGTH;

    snprintf(
        command,
        sizeof(command),
        "AT+QIRD=0,%zu\r\n",
        qird_length
    );

    while (elapsed < timeout_ms) {

        total_received = 0;
        response[0] = '\0';

        ec200_uart_flush();

        if (ec200_uart_write(
                command,
                strlen(command)) != EC200_OK)
            return EC200_ERROR;

        uint32_t query_elapsed = 0;

        while (query_elapsed < 1000 &&
               elapsed + query_elapsed < timeout_ms) {

            if (total_received >= sizeof(response) - 1)
                break;

            int result = ec200_uart_read(
                response + total_received,
                sizeof(response) - 1 - total_received,
                &chunk_received,
                100
            );

            if (result == EC200_OK) {

                total_received += chunk_received;
                response[total_received] = '\0';

                if (total_received >= 6 &&
                    strstr(response + total_received - 6, "\r\nOK\r\n") != NULL)
                    break;

                if (total_received >= 9 &&
                    strstr(response + total_received - 9, "\r\nERROR\r\n") != NULL)
                    return EC200_ERROR;
            }

            query_elapsed += 100;
        }

        char *header = strstr(response, "+QIRD:");

        if (header != NULL) {
            char *header_end = strstr(header, "\r\n");

            if (header_end == NULL)
                return EC200_ERROR;

            *header_end = '\0';

            char *length_text = strchr(header, ':');

            if (length_text == NULL)
                return EC200_ERROR;

            length_text++;

            while (*length_text == ' ')
                length_text++;

            char *separator = strchr(length_text, ',');

            if (separator != NULL)
                length_text = separator + 1;

            size_t data_length = strtoul(length_text, NULL, 10);
            char *data_start = header_end + 2;

            if (data_length > 0) {
                if ((size_t)(data_start - response) > total_received ||
                    data_length > total_received - (size_t)(data_start - response))
                    return EC200_ERROR;

                if (data_length > buffer_size)
                    data_length = buffer_size;

                memcpy(buffer, data_start, data_length);
                *received = data_length;
                return EC200_OK;
            }
        } else if (strstr(response, "ERROR") != NULL ||
                   strstr(response, "+CME ERROR:") != NULL) {
            printf("QIRD modem response: %s\n", response);
            return EC200_ERROR;
        }

        elapsed += query_elapsed;
    }

    return EC200_NO_PAYLOAD;
}


int tcp_close_socket(void)
{
    char response[64];

    return mdm_send_command(
        "AT+QICLOSE=0\r\n",
        response,
        sizeof(response),
        5000
    );
}