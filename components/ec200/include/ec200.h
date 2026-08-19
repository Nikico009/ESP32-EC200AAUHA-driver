#ifndef EC200_H
#define EC200_H

#include <stdbool.h>
#include <stddef.h>

/** @brief HARDWARE_DEFINITIONS */
#define UART_PORT UART_NUM_1
#define LED_STA GPIO_NUM_15
#define LED_AUX GPIO_NUM_25
#define TXD_MICRO GPIO_NUM_17
#define RXD_MICRO GPIO_NUM_16
#define EN_MDM GPIO_NUM_14
#define PWR_KEY GPIO_NUM_13

/** @brief Return codes */
#define EC200_OK       0
#define EC200_ERROR   -1

/** @brief Maximum number of modem initialization retries */
#define EC200_RETRIES  10

/** @brief Claro Argentina APN */
#define CLARO_APN      "igprs.claro.com.ar"

/** @brief Movistar Argentina APN */
#define MOVISTAR_APN   "wap.gprs.unifon.com.ar"

/** @brief Personal Argentina APN */
#define PERSONAL_APN   "internet"


/**
 * @brief Current EC200 modem state.
 */
typedef struct {
    bool modem_communication;
    bool sim_detect;
} modem_state_t;


/**
 * @brief Send an AT command to the EC200 modem.
 *
 * @param cmd Command to send.
 * @param response Buffer where the modem response will be stored.
 * @param response_size Size of the response buffer.
 *
 * @return EC200_OK if the command was sent and a response was received.
 * @return EC200_ERROR on communication failure or timeout.
 */
int send_mdm(const char *cmd, char *response, size_t response_size);


/**
 * @brief Power on and initialize the EC200 modem.
 *
 * @return EC200_OK if the modem responds correctly.
 * @return EC200_ERROR if initialization fails.
 */
int modem_init(void);


/**
 * @brief Check SIM availability and modem network status.
 *
 * @return EC200_OK if the SIM is ready.
 * @return EC200_ERROR if the SIM is unavailable or not ready.
 */
int modem_check_sim(void);


/**
 * @brief Open a TCP socket to a remote host.
 *
 * @param host Remote hostname or IP address.
 * @param port Remote TCP port.
 *
 * @return EC200_OK if the socket was opened successfully.
 * @return EC200_ERROR if the connection fails.
 */
int open_tcp_socket(const char *host, int port);

#endif /* EC200_H */