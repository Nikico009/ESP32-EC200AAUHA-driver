#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "ec200.h"

int send_mdm(const char *cmd, char *response, size_t response_size) {
    char tx_buf[128];

    if (cmd == NULL) {
        return EC200_ERROR;
    }

    if (strchr(cmd, '\r') == NULL && strchr(cmd, '\n') == NULL) {
        snprintf(tx_buf, sizeof(tx_buf), "%s\r\n", cmd);
    } else {
        snprintf(tx_buf, sizeof(tx_buf), "%s", cmd);
    }

    uart_flush_input(UART_PORT);

    int written = uart_write_bytes(UART_PORT, tx_buf, strlen(tx_buf));

    if (written < 0) {
        return EC200_ERROR;
    }

    if (response == NULL || response_size == 0) {
        return EC200_OK;
    }

    memset(response, 0, response_size);

    int len = uart_read_bytes(
        UART_PORT,
        (uint8_t *)response,
        response_size - 1,
        pdMS_TO_TICKS(1000)
    );

    if (len <= 0) {
        return EC200_ERROR;
    }

    response[len] = '\0';

    return EC200_OK;
}

bool modem_init(void)
{
    char response[64];
    uint8_t retries = RETRIES;

    gpio_set_level(EN_MDM, 0);
    gpio_set_level(PWR_KEY, 1);
    vTaskDelay(pdMS_TO_TICKS(5000));

    gpio_set_level(EN_MDM, 1);
    vTaskDelay(pdMS_TO_TICKS(150));

    gpio_set_level(PWR_KEY, 0);
    vTaskDelay(pdMS_TO_TICKS(1800));

    gpio_set_level(PWR_KEY, 1);
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (retries > 0) {

        if (send_mdm("AT", response, sizeof(response)) == EC200_OK) {

            if (strstr(response, "OK") != NULL) {
                gpio_set_level(LED_STA, 1);
                return true;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
        retries--;
    }

    return false;
}

bool modem_check_sim(void)
{
    char response[256];

    send_mdm("AT+QSIMSTAT?", response, sizeof(response));
    send_mdm("AT+QSIMDET=0,0", response, sizeof(response));

    send_mdm("AT+CFUN=0", response, sizeof(response));
    vTaskDelay(pdMS_TO_TICKS(1000));

    send_mdm("AT+CFUN=1", response, sizeof(response));
    vTaskDelay(pdMS_TO_TICKS(3000));

    if (send_mdm("AT+CPIN?", response, sizeof(response)) != EC200_OK) {
        return false;
    }

    if (strstr(response, "READY") == NULL) {
        return false;
    }

    send_mdm("AT+CSQ", response, sizeof(response));
    send_mdm("AT+CREG?", response, sizeof(response));
    send_mdm("AT+COPS?", response, sizeof(response));

    return true;
}