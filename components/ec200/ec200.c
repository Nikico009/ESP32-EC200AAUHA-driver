#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "ec200.h"

bool send_mdm(const char *cmd, const char *expected) {
    char tx_buf[128];
    uint8_t rx_buf[256];
    bool success = false;

    if (strstr(cmd, "\r") == NULL && strstr(cmd, "\n") == NULL) {
        snprintf(tx_buf, sizeof(tx_buf), "%s\r\n", cmd);
    } else {
        snprintf(tx_buf, sizeof(tx_buf), "%s", cmd);
    }

    uart_flush_input(UART_PORT);
    uart_write_bytes(UART_PORT, tx_buf, strlen(tx_buf));

    memset(rx_buf, 0, sizeof(rx_buf));
    int len = uart_read_bytes(UART_PORT, rx_buf, sizeof(rx_buf) - 1, pdMS_TO_TICKS(1000));

    if (len > 0) {
        rx_buf[len] = '\0';
        if (expected != NULL && strstr((char *)rx_buf, expected) != NULL) {
            success = true;
        }
    }

    return success;
}

bool modem_init(void) {
    bool return_value = false;
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

    while (!return_value && retries > 0) {
        if (send_mdm("AT", "OK")) {
            return_value = true;
            gpio_set_level(LED_STA, 1);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        retries--;
    }

    return return_value;
}

bool modem_check_sim(void) {
    send_mdm("AT+QSIMSTAT?", NULL);
    send_mdm("AT+QSIMDET=0,0", NULL);

    send_mdm("AT+CFUN=0", NULL);
    vTaskDelay(pdMS_TO_TICKS(1000));
    send_mdm("AT+CFUN=1", NULL);
    vTaskDelay(pdMS_TO_TICKS(3000));

    bool sim_ready = send_mdm("AT+CPIN?", "READY");

    if (sim_ready) {
        send_mdm("AT+CSQ", NULL);
        send_mdm("AT+CREG?", NULL);
        send_mdm("AT+COPS?", NULL);
    }

    return sim_ready;
}