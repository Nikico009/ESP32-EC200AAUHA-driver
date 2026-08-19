#ifndef EC200_H
#define EC200_H

#include <stdbool.h>

#define RETRIES 10
#define UART_PORT UART_NUM_1
#define LED_STA GPIO_NUM_15
#define LED_AUX GPIO_NUM_25
#define TXD_MICRO GPIO_NUM_17
#define RXD_MICRO GPIO_NUM_16
#define EN_MDM GPIO_NUM_14
#define PWR_KEY GPIO_NUM_13

#define EC200_OK 0
#define EC200_ERROR -1

typedef struct {
    bool modem_communication;
    bool sim_detect;
} modem_state_t;

// Firma limpia con 2 parámetros
int send_mdm(const char *cmd, char *response, size_t response_size);
bool modem_init(void);
bool modem_check_sim(void);

#endif // EC200_H