#ifndef EC200_TCP_H
#define EC200_TCP_H

#include <stddef.h>
#include <stdint.h>


/** @brief Maximum TCP response buffer size */

#define MAX_TCP_RESPONSE 4096


/** @brief TCP return codes */

#define EC200_NO_PAYLOAD 1


/**
 * @brief Open a TCP socket to a remote host.
 *
 * @param host Remote hostname or IP address.
 * @param port Remote TCP port.
 * @param timeout_ms Maximum time to wait for the connection.
 *
 * @return EC200_OK when the socket is opened,
 *         otherwise EC200_ERROR.
 */
int tcp_open_socket(const char *host, int port, uint32_t timeout_ms);


/**
 * @brief Send data through the TCP socket.
 *
 * @param payload Data to transmit.
 * @param payload_length Number of bytes to transmit.
 * @param timeout_ms Maximum time to wait for modem responses.
 *
 * @return EC200_OK when the data is transmitted,
 *         otherwise EC200_ERROR.
 */
int tcp_send_payload(const char *payload, size_t payload_length, uint32_t timeout_ms);


/**
 * @brief Receive data from the TCP socket.
 *
 * @param buffer Destination buffer.
 * @param buffer_size Maximum number of bytes to store.
 * @param received Number of bytes received.
 * @param timeout_ms Maximum time to wait for data.
 *
 * @return EC200_OK when data is received,
 *         EC200_NO_PAYLOAD when no data is available,
 *         otherwise EC200_ERROR.
 */
int tcp_receive(void *buffer, size_t buffer_size, size_t *received, uint32_t timeout_ms);


/**
 * @brief Close the TCP socket.
 *
 * @return EC200_OK when the socket is closed,
 *         otherwise EC200_ERROR.
 */
int tcp_close_socket(void);

#endif /* EC200_TCP_H */