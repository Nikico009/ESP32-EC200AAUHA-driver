#include "EC200_MODEM.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "pins_init.h"


#define MODEM_UART_PORT            MODEM_UART

#define MODEM_AT_TIMEOUT_MS        1000
#define MODEM_SIM_TIMEOUT_MS       2000
#define MODEM_NETWORK_QUERY_MS     2000

#define MODEM_POWER_ON_DELAY_MS    500
#define MODEM_POWER_KEY_MS         1000
#define MODEM_BOOT_TIMEOUT_MS      10000


/**
 * @brief Get the current system time in milliseconds.
 *
 * @return Current system time in milliseconds.
 */
static uint64_t modem_get_time_ms(void) {
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}


/**
 * @brief Check whether a modem response contains an error.
 *
 * @param response Modem response buffer.
 *
 * @return true when an error is detected, otherwise false.
 */
static bool modem_response_has_error(const char *response) {
    if (response == NULL) {
        return true;
    }

    if (strstr(response, "\r\nERROR\r\n") != NULL) {
        return true;
    }

    if (strstr(response, "+CME ERROR:") != NULL) {
        return true;
    }

    if (strstr(response, "+CMS ERROR:") != NULL) {
        return true;
    }

    return false;
}


/**
 * @brief Initialize the modem UART interface.
 */
int ec200_uart_init(void) {
    uart_config_t uart_config = {
        .baud_rate = EC200_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    esp_err_t result = uart_driver_install(
        MODEM_UART_PORT,
        EC200_UART_RX_BUFFER_SIZE,
        EC200_UART_TX_BUFFER_SIZE,
        0,
        NULL,
        0
    );

    if (result != ESP_OK) {
        return EC200_ERROR;
    }

    result = uart_param_config(
        MODEM_UART_PORT,
        &uart_config
    );

    if (result != ESP_OK) {
        uart_driver_delete(MODEM_UART_PORT);
        return EC200_ERROR;
    }

    result = uart_set_pin(
        MODEM_UART_PORT,
        MODEM_TX,
        MODEM_RX,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    );

    if (result != ESP_OK) {
        uart_driver_delete(MODEM_UART_PORT);
        return EC200_ERROR;
    }

    return EC200_OK;
}


/**
 * @brief Send raw data through the modem UART.
 */
int ec200_uart_write(const void *data, size_t length) {
    if (data == NULL || length == 0) {
        return EC200_ERROR;
    }

    int written = uart_write_bytes(
        MODEM_UART_PORT,
        data,
        length
    );

    if (written < 0 || (size_t)written != length) {
        return EC200_ERROR;
    }

    return EC200_OK;
}


/**
 * @brief Receive raw data from the modem UART.
 */
int ec200_uart_read(void *buffer, size_t buffer_size, size_t *received, uint32_t timeout_ms) {
    if (buffer == NULL || buffer_size == 0 || received == NULL) {
        return EC200_ERROR;
    }

    *received = 0;

    int length = uart_read_bytes(
        MODEM_UART_PORT,
        buffer,
        buffer_size,
        pdMS_TO_TICKS(timeout_ms)
    );

    if (length < 0) {
        return EC200_ERROR;
    }

    if (length == 0) {
        return EC200_UART_TIMEOUT;
    }

    *received = (size_t)length;

    return EC200_OK;
}


/**
 * @brief Flush the UART receive buffer.
 */
int ec200_uart_flush(void) {
    if (uart_flush_input(MODEM_UART_PORT) != ESP_OK) {
        return EC200_ERROR;
    }

    return EC200_OK;
}


/**
 * @brief Deinitialize the modem UART.
 */
int ec200_uart_deinit(void) {
    uart_driver_delete(MODEM_UART_PORT);

    return EC200_OK;
}


/**
 * @brief Wait for a specific response from the modem.
 */
int modem_wait_response(const char *expected, char *response, size_t response_size, uint32_t timeout_ms) {
    if (expected == NULL || response == NULL || response_size < 2) {
        return EC200_ERROR;
    }

    response[0] = '\0';

    size_t total_received = 0;
    uint64_t start_time = modem_get_time_ms();

    while (1) {
        uint64_t elapsed = modem_get_time_ms() - start_time;

        if (elapsed >= timeout_ms) {
            return EC200_UART_TIMEOUT;
        }

        uint32_t remaining = timeout_ms - (uint32_t)elapsed;
        uint32_t read_timeout = remaining;

        if (read_timeout > 100) {
            read_timeout = 100;
        }

        if (total_received >= response_size - 1) {
            return EC200_ERROR;
        }

        size_t received = 0;

        int result = ec200_uart_read(
            response + total_received,
            response_size - 1 - total_received,
            &received,
            read_timeout
        );

        if (result == EC200_OK) {
            total_received += received;
            response[total_received] = '\0';

            if (modem_response_has_error(response)) {
                return EC200_ERROR;
            }

            if (strstr(response, expected) != NULL) {
                return EC200_OK;
            }
        } else if (result != EC200_UART_TIMEOUT) {
            return EC200_ERROR;
        }
    }
}


/**
 * @brief Send an AT command and wait for an OK response.
 */
int modem_send_command(const char *command, char *response, size_t response_size, uint32_t timeout_ms) {
    if (command == NULL || response == NULL || response_size < 2) {
        return EC200_ERROR;
    }

    char command_buffer[256];

    int length = snprintf(
        command_buffer,
        sizeof(command_buffer),
        "%s\r\n",
        command
    );

    if (length < 0 || (size_t)length >= sizeof(command_buffer)) {
        return EC200_ERROR;
    }

    if (ec200_uart_flush() != EC200_OK) {
        return EC200_ERROR;
    }

    if (ec200_uart_write(command_buffer, (size_t)length) != EC200_OK) {
        return EC200_ERROR;
    }

    return modem_wait_response(
        "\r\nOK\r\n",
        response,
        response_size,
        timeout_ms
    );
}


/**
 * @brief Power on and initialize the EC200 modem.
 */
int modem_init(void) {
    char response[128];

    /* EC200 power-on sequence is typically active-low on PWR_KEY.
     * Keep the signal idle high and generate a low pulse for ~1s. */
    gpio_set_level(MDM_EN, 1);

    vTaskDelay(pdMS_TO_TICKS(MODEM_POWER_ON_DELAY_MS));

    gpio_set_level(PWR_KEY, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    gpio_set_level(PWR_KEY, 0);

    vTaskDelay(pdMS_TO_TICKS(MODEM_POWER_KEY_MS));

    gpio_set_level(PWR_KEY, 1);

    uint64_t start_time = modem_get_time_ms();

    for (int retry = 0; retry < EC200_RETRIES; retry++) {
        if (modem_get_time_ms() - start_time >= MODEM_BOOT_TIMEOUT_MS) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(500));

        if (modem_send_command(
                "AT",
                response,
                sizeof(response),
                MODEM_AT_TIMEOUT_MS
            ) == EC200_OK) {
            return EC200_OK;
        }
    }

    return EC200_ERROR;
}


/**
 * @brief Check whether the SIM card is detected and ready.
 */
int modem_check_sim(void) {
    char response[256];

    for (int retry = 0; retry < 5; retry++) {
        if (modem_send_command(
                "AT+CPIN?",
                response,
                sizeof(response),
                MODEM_SIM_TIMEOUT_MS
            ) == EC200_OK) {

            if (strstr(response, "+CPIN: READY") != NULL) {
                return EC200_OK;
            }

            if (strstr(response, "+CPIN: SIM PIN") != NULL ||
                strstr(response, "+CPIN: SIM PUK") != NULL ||
                strstr(response, "+CPIN: PH_SIM PIN") != NULL ||
                strstr(response, "+CPIN: PH_SIM PUK") != NULL ||
                strstr(response, "+CPIN: NOT READY") != NULL ||
                strstr(response, "+CME ERROR: 10") != NULL ||
                strstr(response, "+CME ERROR: 11") != NULL ||
                strstr(response, "NO SIM") != NULL ||
                strstr(response, "NOT INSERTED") != NULL) {
                return EC200_ERROR;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    return EC200_ERROR;
}


/**
 * @brief Wait until the modem is registered on the cellular network.
 */
int modem_wait_network(uint32_t timeout_ms) {
    char response[256];

    uint64_t start_time = modem_get_time_ms();

    while (1) {
        uint64_t elapsed = modem_get_time_ms() - start_time;

        if (elapsed >= timeout_ms) {
            return EC200_ERROR;
        }

        uint32_t remaining = timeout_ms - (uint32_t)elapsed;
        uint32_t query_timeout = MODEM_NETWORK_QUERY_MS;

        if (query_timeout > remaining) {
            query_timeout = remaining;
        }

        if (modem_send_command(
                "AT+CEREG?",
                response,
                sizeof(response),
                query_timeout
            ) == EC200_OK) {

            char *cereg = strstr(response, "+CEREG:");

            if (cereg != NULL) {
                int n;
                int status;

                if (sscanf(cereg, "+CEREG: %d,%d", &n, &status) == 2) {
                    if (status == 1 || status == 5) {
                        return EC200_OK;
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


/**
 * @brief Configure the PDP context with an APN.
 */
int modem_configure_apn(const char *apn) {
    if (apn == NULL || apn[0] == '\0') {
        return EC200_ERROR;
    }

    char command[256];
    char response[128];

    int length = snprintf(
        command,
        sizeof(command),
        "AT+QICSGP=%d,1,\"%s\",\"\",\"\",0",
        EC200_PDP_CONTEXT_ID,
        apn
    );

    if (length < 0 || (size_t)length >= sizeof(command)) {
        return EC200_ERROR;
    }

    if (modem_send_command(
            command,
            response,
            sizeof(response),
            2000
        ) != EC200_OK) {
        return EC200_ERROR;
    }

    return EC200_OK;
}


/**
 * @brief Activate the configured PDP context.
 */
int modem_activate_pdp(uint32_t timeout_ms) {
    char command[64];
    char response[256];

    snprintf(
        command,
        sizeof(command),
        "AT+QIACT=%d",
        EC200_PDP_CONTEXT_ID
    );

    if (modem_send_command(
            command,
            response,
            sizeof(response),
            timeout_ms
        ) != EC200_OK) {
        return EC200_ERROR;
    }

    return modem_check_pdp();
}


/**
 * @brief Check whether the PDP context is active.
 */
int modem_check_pdp(void) {
    char response[512];
    char expected[32];

    if (modem_send_command(
            "AT+QIACT?",
            response,
            sizeof(response),
            2000
        ) != EC200_OK) {
        return EC200_ERROR;
    }

    snprintf(
        expected,
        sizeof(expected),
        "+QIACT: %d,",
        EC200_PDP_CONTEXT_ID
    );

    if (strstr(response, expected) == NULL) {
        return EC200_ERROR;
    }

    return EC200_OK;
}


/**
 * @brief Establish the complete cellular data connection.
 */
int modem_connect_network(const char *apn, uint32_t timeout_ms) {
    if (apn == NULL || apn[0] == '\0') {
        return EC200_ERROR;
    }

    uint64_t start_time = modem_get_time_ms();

    if (modem_check_sim() != EC200_OK) {
        return EC200_ERROR;
    }

    uint64_t elapsed = modem_get_time_ms() - start_time;

    if (elapsed >= timeout_ms) {
        return EC200_ERROR;
    }

    if (modem_wait_network(
            timeout_ms - (uint32_t)elapsed
        ) != EC200_OK) {
        return EC200_ERROR;
    }

    elapsed = modem_get_time_ms() - start_time;

    if (elapsed >= timeout_ms) {
        return EC200_ERROR;
    }

    if (modem_configure_apn(apn) != EC200_OK) {
        return EC200_ERROR;
    }

    elapsed = modem_get_time_ms() - start_time;

    if (elapsed >= timeout_ms) {
        return EC200_ERROR;
    }

    if (modem_activate_pdp(
            timeout_ms - (uint32_t)elapsed
        ) != EC200_OK) {
        return EC200_ERROR;
    }

    return EC200_OK;
}