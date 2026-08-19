#ifndef PINS_INIT_H
#define PINS_INIT_H

#include "driver/gpio.h"


/** @brief HARDWARE_DEFINITIONS */
#define UART_PORT UART_NUM_1
#define LED_STA GPIO_NUM_15
#define LED_AUX GPIO_NUM_25
#define TXD_MICRO GPIO_NUM_17
#define RXD_MICRO GPIO_NUM_16
#define EN_MDM GPIO_NUM_14
#define PWR_KEY GPIO_NUM_13

/**
 * @brief Configure a GPIO pin as a digital output.
 *
 * @param gpio GPIO pin to configure.
 */
void set_output(gpio_num_t gpio);


/**
 * @brief Initialize the modem UART and required GPIO pins.
 */
void pins_init(void);

#endif /* PINS_INIT_H */