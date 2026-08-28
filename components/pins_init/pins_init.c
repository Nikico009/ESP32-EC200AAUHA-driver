#include "pins_init.h"

#include "driver/gpio.h"


void set_output(gpio_num_t gpio)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf);
}


void pins_init(void)
{
    set_output(LED_STA);
    set_output(LED_AUX);
    set_output(MDM_EN);
    set_output(PWR_KEY);

    gpio_set_level(LED_STA, 0);
    gpio_set_level(LED_AUX, 0);

    /* Modem disabled */
    gpio_set_level(MDM_EN, 0);

    /* PWR_KEY inactive */
    gpio_set_level(PWR_KEY, 1);
}