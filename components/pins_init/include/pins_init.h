#ifndef PINS_INIT_H
#define PINS_INIT_H

#include "driver/gpio.h"
#include "driver/uart.h"


/** @brief HARDWARE DEFINITIONS */

#define MODEM_UART  UART_NUM_1
#define MODEM_TX    GPIO_NUM_17
#define MODEM_RX    GPIO_NUM_16

#define LED_STA     GPIO_NUM_15
#define LED_AUX     GPIO_NUM_25

#define MDM_EN      GPIO_NUM_14
#define PWR_KEY     GPIO_NUM_13


/**
 * @brief Configure a GPIO pin as a digital output.
 *
 * @param gpio GPIO pin to configure.
 */
void set_output(gpio_num_t gpio);


/**
 * @brief Initialize modem control and status GPIOs.
 */
void pins_init(void);

#endif /* PINS_INIT_H */