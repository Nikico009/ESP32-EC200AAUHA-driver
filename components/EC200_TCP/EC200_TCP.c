#include "EC200_TCP.h"

#include "EC200_MODEM.h"

#include "esp_timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define TCP_SOCKET_ID          0
#define TCP_QIRD_MAX_LENGTH   1024
#define TCP_QIRD_OVERHEAD     64


/**
 * @brief Open a TCP socket to a remote host.
 */
int tcp_open_socket(const char *host, int port, uint32_t timeout_ms) {
    if (host == NULL || host[0] == '\0' || port <= 0 || port > 65535) {
        return EC200_ERROR;
    }

    char command[256];
    char response[512];

    int length = snprintf(
        command,
        sizeof(command),
        "AT+QIOPEN=1,%d,\"TCP\",\"%s\",%d,0,0\r\n",
        TCP_SOCKET_ID,
        host,
        port
    );

    if (length < 0 || (size_t)length >= sizeof(command)) {
        return EC200_ERROR;
    }

    if (ec200_uart_flush() != EC200_OK) {
        return EC200_ERROR;
    }

    if (ec200_uart_write(command, (size_t)length) != EC200_OK) {
        return EC200_ERROR;
    }

    if (modem_wait_response(
            "+QIOPEN:",
            response,
            sizeof(response),
            timeout_ms
        ) != EC200_OK) {
        return EC200_ERROR;
    }

    int socket_id = -1;
    int status = -1;

    if (sscanf(response, "%*[^:]: %d,%d", &socket_id, &status) == 2) {
        if (socket_id == TCP_SOCKET_ID && status == 0) {
            return EC200_OK;
        }
    }

    return EC200_ERROR;
}


/**
 * @brief Send data through the TCP socket.
 */
int tcp_send_payload(const char *payload, size_t payload_length, uint32_t timeout_ms) {
    if (payload == NULL || payload_length == 0) {
        return EC200_ERROR;
    }

    char command[64];
    char response[256];

    int length = snprintf(
        command,
        sizeof(command),
        "AT+QISEND=%d,%zu\r\n",
        TCP_SOCKET_ID,
        payload_length
    );

    if (length < 0 || (size_t)length >= sizeof(command)) {
        return EC200_ERROR;
    }

    if (ec200_uart_flush() != EC200_OK) {
        return EC200_ERROR;
    }

    if (ec200_uart_write(command, (size_t)length) != EC200_OK) {
        return EC200_ERROR;
    }

    if (modem_wait_response(
            ">",
            response,
            sizeof(response),
            timeout_ms
        ) != EC200_OK) {
        return EC200_ERROR;
    }

    if (ec200_uart_write(
            payload,
            payload_length
        ) != EC200_OK) {
        return EC200_ERROR;
    }

    if (modem_wait_response(
            "SEND OK",
            response,
            sizeof(response),
            timeout_ms
        ) != EC200_OK) {
        return EC200_ERROR;
    }

    return EC200_OK;
}


/**
 * @brief Receive data from the TCP socket.
 */
int tcp_receive(void *buffer, size_t buffer_size, size_t *received, uint32_t timeout_ms) {
    if (buffer == NULL || received == NULL || buffer_size == 0) {
        return EC200_ERROR;
    }

    *received = 0;

    size_t qird_length = buffer_size;

    if (qird_length > TCP_QIRD_MAX_LENGTH) {
        qird_length = TCP_QIRD_MAX_LENGTH;
    }

    char command[64];

    int command_length = snprintf(
        command,
        sizeof(command),
        "AT+QIRD=%d,%zu\r\n",
        TCP_SOCKET_ID,
        qird_length
    );

    if (command_length < 0 ||
        (size_t)command_length >= sizeof(command)) {
        return EC200_ERROR;
    }

    uint64_t start_time =
        (uint64_t)(esp_timer_get_time() / 1000ULL);

    while (1) {
        uint64_t now =
            (uint64_t)(esp_timer_get_time() / 1000ULL);

        uint64_t elapsed = now - start_time;

        if (elapsed >= timeout_ms) {
            return EC200_NO_PAYLOAD;
        }

        uint32_t remaining =
            timeout_ms - (uint32_t)elapsed;

        uint32_t query_timeout = remaining;

        if (query_timeout > 1000) {
            query_timeout = 1000;
        }

        char response[
            TCP_QIRD_MAX_LENGTH +
            TCP_QIRD_OVERHEAD
        ];

        response[0] = '\0';

        if (ec200_uart_flush() != EC200_OK) {
            return EC200_ERROR;
        }

        if (ec200_uart_write(
                command,
                (size_t)command_length
            ) != EC200_OK) {
            return EC200_ERROR;
        }

        size_t total_received = 0;

        uint64_t query_start =
            (uint64_t)(esp_timer_get_time() / 1000ULL);

        while (1) {
            uint64_t query_now =
                (uint64_t)(esp_timer_get_time() / 1000ULL);

            uint64_t query_elapsed =
                query_now - query_start;

            if (query_elapsed >= query_timeout) {
                break;
            }

            if (total_received >= sizeof(response) - 1) {
                return EC200_ERROR;
            }

            uint32_t read_timeout =
                query_timeout - (uint32_t)query_elapsed;

            if (read_timeout > 100) {
                read_timeout = 100;
            }

            size_t chunk_received = 0;

            int result = ec200_uart_read(
                response + total_received,
                sizeof(response) - 1 - total_received,
                &chunk_received,
                read_timeout
            );

            if (result == EC200_OK) {
                total_received += chunk_received;
                response[total_received] = '\0';

                if (strstr(
                        response,
                        "\r\nOK\r\n"
                    ) != NULL) {
                    break;
                }

                if (strstr(
                        response,
                        "\r\nERROR\r\n"
                    ) != NULL) {
                    return EC200_ERROR;
                }

                if (strstr(
                        response,
                        "+CME ERROR:"
                    ) != NULL) {
                    return EC200_ERROR;
                }
            }
        }

        char *header = strstr(
            response,
            "+QIRD:"
        );

        if (header == NULL) {
            vTaskDelay(
                pdMS_TO_TICKS(100)
            );

            continue;
        }

        char *header_end = strstr(
            header,
            "\r\n"
        );

        if (header_end == NULL) {
            return EC200_ERROR;
        }

        char *length_text =
            header + strlen("+QIRD:");

        while (*length_text == ' ') {
            length_text++;
        }

        char *endptr = NULL;

        unsigned long data_length =
            strtoul(
                length_text,
                &endptr,
                10
            );

        if (endptr == length_text) {
            return EC200_ERROR;
        }

        char *data_start =
            header_end + 2;

        size_t header_offset =
            (size_t)(data_start - response);

        if (header_offset > total_received) {
            return EC200_ERROR;
        }

        if (data_length == 0) {
            vTaskDelay(
                pdMS_TO_TICKS(100)
            );

            continue;
        }

        if ((size_t)data_length >
            total_received - header_offset) {
            return EC200_ERROR;
        }

        size_t copy_length =
            (size_t)data_length;

        if (copy_length > buffer_size) {
            copy_length = buffer_size;
        }

        memcpy(
            buffer,
            data_start,
            copy_length
        );

        *received = copy_length;

        return EC200_OK;
    }
}


/**
 * @brief Close the TCP socket.
 */
int tcp_close_socket(void) {
    char response[128];
    char command[64];

    snprintf(
        command,
        sizeof(command),
        "AT+QICLOSE=%d",
        TCP_SOCKET_ID
    );

    return modem_send_command(
        command,
        response,
        sizeof(response),
        5000
    );
}