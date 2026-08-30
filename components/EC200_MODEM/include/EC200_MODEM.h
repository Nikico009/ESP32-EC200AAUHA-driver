#ifndef EC200_MODEM_H
#define EC200_MODEM_H

#include <stddef.h>
#include <stdint.h>


/** @brief UART configuration */

#define EC200_UART_BAUD_RATE       115200
#define EC200_UART_RX_BUFFER_SIZE  2048
#define EC200_UART_TX_BUFFER_SIZE  2048

#define EC200_RETRIES              10


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
int ec200_uart_write(
    const void *data,
    size_t length
);


/**
 * @brief Receive raw data from the modem UART.
 *
 * @param buffer Destination buffer.
 * @param buffer_size Buffer size.
 * @param received Number of bytes received.
 * @param timeout_ms Timeout in milliseconds.
 *
 * @return EC200_OK on success,
 *         EC200_UART_TIMEOUT on timeout,
 *         otherwise EC200_ERROR.
 */
int ec200_uart_read(
    void *buffer,
    size_t buffer_size,
    size_t *received,
    uint32_t timeout_ms
);


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

#endif /* EC200_MODEM_H */