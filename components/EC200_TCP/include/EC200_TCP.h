#ifndef EC200_TCP_H
#define EC200_TCP_H

#include <stddef.h>
#include <stdint.h>

#include "EC200_MODEM.h"


/** @brief TCP configuration */

#define MAX_TCP_RESPONSE 4096


/** @brief TCP return codes */

#define EC200_NO_PAYLOAD 1


/**
 * @brief Open a TCP socket to a remote host.
 *
 * @param host Remote hostname or IP address.
 * @param port Remote TCP port.
 * @param timeout_ms Timeout in milliseconds.
 *
 * @return EC200_OK on success, otherwise EC200_ERROR.
 */
int tcp_open_socket(
    const char *host,
    int port,
    uint32_t timeout_ms
);


/**
 * @brief Send raw data through the TCP socket.
 *
 * @param payload Data to send.
 * @param payload_length Number of bytes.
 * @param timeout_ms Timeout in milliseconds.
 *
 * @return EC200_OK on success, otherwise EC200_ERROR.
 */
int tcp_send_payload(
    const char *payload,
    size_t payload_length,
    uint32_t timeout_ms
);


/**
 * @brief Receive data from the TCP socket.
 *
 * @param buffer Destination buffer.
 * @param buffer_size Buffer size.
 * @param received Number of bytes received.
 * @param timeout_ms Timeout in milliseconds.
 *
 * @return EC200_OK on success,
 *         EC200_NO_PAYLOAD if no data is available,
 *         otherwise EC200_ERROR.
 */
int tcp_receive(
    void *buffer,
    size_t buffer_size,
    size_t *received,
    uint32_t timeout_ms
);


/**
 * @brief Close the TCP socket.
 *
 * @return EC200_OK on success, otherwise EC200_ERROR.
 */
int tcp_close_socket(void);

#endif /* EC200_TCP_H */