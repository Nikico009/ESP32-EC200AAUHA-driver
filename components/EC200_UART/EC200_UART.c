#include "EC200_UART.h"

#include "pins_init.h"

#include "driver/gpio.h"
#include "driver/uart.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>


#define MODEM_BOOT_DELAY_MS     5000
#define MODEM_POWER_DELAY_MS     500
#define PWR_KEY_PULSE_MS         800

#define MODEM_COMMAND_TIMEOUT_MS 1000
#define MODEM_SIM_TIMEOUT_MS     3000


int ec200_uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = EC200_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    if (uart_param_config(
            MODEM_UART,
            &uart_config) != ESP_OK)
        return EC200_ERROR;

    if (uart_set_pin(
            MODEM_UART,
            MODEM_TX,
            MODEM_RX,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE) != ESP_OK)
        return EC200_ERROR;

    if (uart_driver_install(
            MODEM_UART,
            EC200_UART_RX_BUFFER_SIZE,
            EC200_UART_TX_BUFFER_SIZE,
            0,
            NULL,
            0) != ESP_OK)
        return EC200_ERROR;

    return EC200_OK;
}


int ec200_uart_write(
    const void *data,
    size_t length
)
{
    if (data == NULL || length == 0)
        return EC200_ERROR;

    int bytes_written = uart_write_bytes(
        MODEM_UART,
        data,
        length
    );

    if (bytes_written < 0 ||
        (size_t)bytes_written != length)
        return EC200_ERROR;

    return EC200_OK;
}


int ec200_uart_read(
    void *buffer,
    size_t buffer_size,
    size_t *received,
    uint32_t timeout_ms
)
{
    if (buffer == NULL ||
        received == NULL ||
        buffer_size == 0)
        return EC200_ERROR;

    *received = 0;

    int bytes_read = uart_read_bytes(
        MODEM_UART,
        buffer,
        buffer_size,
        pdMS_TO_TICKS(timeout_ms)
    );

    if (bytes_read < 0)
        return EC200_ERROR;

    if (bytes_read == 0)
        return EC200_UART_TIMEOUT;

    *received = (size_t)bytes_read;

    return EC200_OK;
}


int ec200_uart_flush(void)
{
    if (uart_flush_input(MODEM_UART) != ESP_OK)
        return EC200_ERROR;

    return EC200_OK;
}


int ec200_uart_deinit(void)
{
    if (uart_driver_delete(MODEM_UART) != ESP_OK)
        return EC200_ERROR;

    return EC200_OK;
}


/**
 * @brief Send an AT command and wait for a response.
 */
static int modem_command(
    const char *command,
    char *response,
    size_t response_size,
    uint32_t timeout_ms
)
{
    size_t received = 0;

    if (command == NULL ||
        response == NULL ||
        response_size < 2)
        return EC200_ERROR;

    if (ec200_uart_write(
            command,
            strlen(command)) != EC200_OK)
        return EC200_ERROR;

    if (ec200_uart_read(
            response,
            response_size - 1,
            &received,
            timeout_ms) != EC200_OK)
        return EC200_ERROR;

    response[received] = '\0';

    return EC200_OK;
}


/**
 * @brief Power on the EC200 modem.
 */
static void modem_power_on(void)
{
    /*
     * Enable modem power.
     */
    gpio_set_level(MDM_EN, 1);

    vTaskDelay(
        pdMS_TO_TICKS(MODEM_POWER_DELAY_MS)
    );

    /*
     * PWR_KEY is active-low.
     */
    gpio_set_level(PWR_KEY, 0);

    vTaskDelay(
        pdMS_TO_TICKS(PWR_KEY_PULSE_MS)
    );

    gpio_set_level(PWR_KEY, 1);

    /*
     * Wait for modem boot.
     */
    vTaskDelay(
        pdMS_TO_TICKS(MODEM_BOOT_DELAY_MS)
    );
}


int modem_init(void)
{
    char response[128];

    modem_power_on();

    for (int attempt = 0;
         attempt < EC200_RETRIES;
         attempt++) {

        ec200_uart_flush();

        if (modem_command(
                "AT\r\n",
                response,
                sizeof(response),
                MODEM_COMMAND_TIMEOUT_MS) == EC200_OK) {

            if (strstr(response, "OK") != NULL)
                return EC200_OK;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    return EC200_ERROR;
}


int modem_check_sim(void)
{
    char response[128];

    ec200_uart_flush();

    if (modem_command(
            "AT+CPIN?\r\n",
            response,
            sizeof(response),
            MODEM_SIM_TIMEOUT_MS) != EC200_OK)
        return EC200_ERROR;

    if (strstr(response, "+CPIN: READY") == NULL)
        return EC200_ERROR;

    return EC200_OK;
}