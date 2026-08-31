#ifndef EC200_MODEM_H
#define EC200_MODEM_H

#include <stddef.h>
#include <stdint.h>


/** @brief UART configuration */

#define EC200_UART_BAUD_RATE       115200
#define EC200_UART_RX_BUFFER_SIZE  2048
#define EC200_UART_TX_BUFFER_SIZE  2048

#define EC200_RETRIES              10


/** @brief PDP context configuration */

#define EC200_PDP_CONTEXT_ID       1


/** @brief Return codes */

#define EC200_OK            0
#define EC200_ERROR        -1
#define EC200_UART_TIMEOUT -2


/**
 * @brief Initialize the modem UART interface.
 *
 * @return EC200_OK on success, otherwise EC200_ERROR.
 */
int ec200_uart_init(void);


/**
 * @brief Send raw data through the modem UART.
 *
 * @param data Data to send.
 * @param length Number of bytes to send.
 *
 * @return EC200_OK on success, otherwise EC200_ERROR.
 */
int ec200_uart_write(const void *data, size_t length);


/**
 * @brief Receive raw data from the modem UART.
 *
 * @param buffer Destination buffer.
 * @param buffer_size Buffer size.
 * @param received Number of bytes received.
 * @param timeout_ms Maximum time to wait for data.
 *
 * @return EC200_OK on success,
 *         EC200_UART_TIMEOUT on timeout,
 *         otherwise EC200_ERROR.
 */
int ec200_uart_read(void *buffer, size_t buffer_size, size_t *received, uint32_t timeout_ms);


/**
 * @brief Flush the UART receive buffer.
 *
 * @return EC200_OK on success, otherwise EC200_ERROR.
 */
int ec200_uart_flush(void);


/**
 * @brief Deinitialize the modem UART.
 *
 * @return EC200_OK on success, otherwise EC200_ERROR.
 */
int ec200_uart_deinit(void);


/**
 * @brief Wait for a specific response from the modem.
 *
 * The received modem data is stored in the supplied buffer.
 *
 * @param expected String that must be detected in the response.
 * @param response Buffer where the response is stored.
 * @param response_size Response buffer size.
 * @param timeout_ms Maximum time to wait.
 *
 * @return EC200_OK when the expected response is received,
 *         EC200_UART_TIMEOUT when the timeout expires,
 *         otherwise EC200_ERROR.
 */
int modem_wait_response(const char *expected, char *response, size_t response_size, uint32_t timeout_ms);


/**
 * @brief Send an AT command and wait for an OK response.
 *
 * The command must not contain the final "\r\n".
 *
 * @param command AT command to send.
 * @param response Buffer where the modem response is stored.
 * @param response_size Response buffer size.
 * @param timeout_ms Maximum time to wait.
 *
 * @return EC200_OK when the modem returns OK,
 *         EC200_UART_TIMEOUT on timeout,
 *         otherwise EC200_ERROR.
 */
int modem_send_command(const char *command, char *response, size_t response_size, uint32_t timeout_ms);


/**
 * @brief Power on and initialize the EC200 modem.
 *
 * @return EC200_OK when the modem responds to AT,
 *         otherwise EC200_ERROR.
 */
int modem_init(void);


/**
 * @brief Check whether the SIM card is detected and ready.
 *
 * @return EC200_OK when the SIM is ready,
 *         otherwise EC200_ERROR.
 */
int modem_check_sim(void);


/**
 * @brief Wait until the modem is registered on the cellular network.
 *
 * The function accepts both home-network registration and
 * roaming registration as successful states.
 *
 * @param timeout_ms Maximum time to wait for registration.
 *
 * @return EC200_OK when the modem is registered,
 *         otherwise EC200_ERROR.
 */
int modem_wait_network(uint32_t timeout_ms);


/**
 * @brief Configure the PDP context with an APN.
 *
 * @param apn Access Point Name provided by the operator.
 *
 * @return EC200_OK on success, otherwise EC200_ERROR.
 */
int modem_configure_apn(const char *apn);


/**
 * @brief Activate the configured PDP context.
 *
 * @param timeout_ms Maximum time to wait for activation.
 *
 * @return EC200_OK when the PDP context is active,
 *         otherwise EC200_ERROR.
 */
int modem_activate_pdp(uint32_t timeout_ms);


/**
 * @brief Check whether the PDP context is active.
 *
 * @return EC200_OK when the PDP context is active,
 *         otherwise EC200_ERROR.
 */
int modem_check_pdp(void);


/**
 * @brief Establish the complete cellular data connection.
 *
 * The function checks the SIM, waits for network registration,
 * configures the APN and activates the PDP context.
 *
 * @param apn Access Point Name provided by the operator.
 * @param timeout_ms Maximum total time for the operation.
 *
 * @return EC200_OK when the data connection is established,
 *         otherwise EC200_ERROR.
 */
int modem_connect_network(const char *apn, uint32_t timeout_ms);

#endif /* EC200_MODEM_H */