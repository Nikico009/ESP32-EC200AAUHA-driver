#ifndef EC200_TCP_H
#define EC200_TCP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"

/** @brief HARDWARE DEFINITIONS */
#define UART_PORT UART_NUM_1

#define LED_STA   GPIO_NUM_15
#define LED_AUX   GPIO_NUM_25

#define TXD_MICRO GPIO_NUM_17
#define RXD_MICRO GPIO_NUM_16

#define EN_MDM    GPIO_NUM_14
#define PWR_KEY   GPIO_NUM_13

/** @brief Return codes */
#define EC200_OK           0
#define EC200_ERROR       -1
#define EC200_NO_PAYLOAD  -2

/** @brief Maximum number of modem initialization retries */
#define EC200_RETRIES 10

/** @brief APNs */
#define CLARO_APN     "igprs.claro.com.ar"
#define MOVISTAR_APN  "wap.gprs.unifon.com.ar"
#define PERSONAL_APN  "internet"

/** @brief Buffer sizes */
#define MAX_TCP_RESPONSE 1024
#define MAX_TCP_PAYLOAD  256
#define MAX_CMD          64
#define MAX_AT_RESPONSE  128

/**
 * @brief Current EC200 modem state.
 */
typedef struct {
    int modem_communication;
    int sim_detect;
    int tcp_socket;
} modem_state_t;

/**
 * @brief Send an AT command and wait for a short response.
 *
 * @param cmd Command to send.
 * @param response Buffer where the response will be stored.
 * @param response_size Size of the response buffer.
 *
 * @return EC200_OK on success.
 * @return EC200_ERROR on communication failure or timeout.
 */
int mdm_send_cmd(const char *cmd, char *response, size_t response_size);

/**
 * @brief Send an AT command and collect its complete response.
 *
 * @param cmd Command to send.
 * @param response Buffer where the response will be stored.
 * @param response_size Size of the response buffer.
 * @param timeout_ms Maximum time to wait for the response.
 *
 * @return EC200_OK on success.
 * @return EC200_ERROR on communication failure or timeout.
 */
int mdm_request_cmd(const char *cmd, char *response, size_t response_size, uint32_t timeout_ms);

/**
 * @brief Power on and initialize the EC200 modem.
 *
 * @return EC200_OK if the modem responds correctly.
 * @return EC200_ERROR if initialization fails.
 */
int modem_init(void);

/**
 * @brief Check SIM availability and automatically select its APN.
 *
 * Detects the network reported by the modem and selects the
 * corresponding APN for Claro, Movistar or Personal.
 *
 * @return EC200_OK if the SIM is ready and an APN was selected.
 * @return EC200_ERROR if the SIM or operator cannot be detected.
 */
int modem_check_sim(void);

/**
 * @brief Open a TCP socket to a remote host.
 *
 * @param host Remote hostname or IP address.
 * @param port Remote TCP port.
 * @param timeout_ms Timeout for socket opening.
 *
 * @return EC200_OK if the socket was opened successfully.
 * @return EC200_ERROR if the connection fails.
 */
int tcp_open_socket(const char *host, int port, uint32_t timeout_ms);

/**
 * @brief Send a raw payload through the TCP socket.
 *
 * @param payload Payload to send.
 * @param payload_length Payload length in bytes.
 * @param timeout_ms Timeout for the operation.
 *
 * @return EC200_OK if the payload was sent successfully.
 * @return EC200_ERROR if the transmission fails.
 */
int tcp_send_payload(const char *payload, size_t payload_length, uint32_t timeout_ms);

/**
 * @brief Receive raw data from the TCP socket.
 *
 * @param buffer Buffer where received data will be stored.
 * @param buffer_size Size of the receiving buffer.
 * @param received Number of bytes actually received.
 * @param timeout_ms Maximum time to wait for data.
 *
 * @return EC200_OK if data was received.
 * @return EC200_NO_PAYLOAD if no data is available.
 * @return EC200_ERROR on communication or parsing failure.
 */
int tcp_receive(void *buffer, size_t buffer_size, size_t *received, uint32_t timeout_ms);

/**
 * @brief Close the TCP socket.
 *
 * @return EC200_OK if the socket was closed successfully.
 * @return EC200_ERROR if the operation fails.
 */
int tcp_close_socket(void);

#endif /* EC200_TCP_H */