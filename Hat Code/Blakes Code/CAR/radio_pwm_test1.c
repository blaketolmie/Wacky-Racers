#include <stdio.h>

#include "usb_serial.h"
#include "pio.h"
#include "pacer.h"
#include "button.h"
#include "panic.h"
#include "target.h"
#include "racer_motors.h"
#include "radio_link.h"

#define BUTTON_POLL_RATE 100

static const button_cfg_t button_cfg =
{
    .pio = BUTTON_PIO
};

static void print_startup(void)
{
    printf("\r\nWacky Racer radio PWM motor control ready.\r\n");
    printf("RX input format: left right\r\n");
    printf("Duty range: -100 to 100\r\n");
    printf("Button sends STOP to the hat and stops the motors\r\n");
    fflush(stdout);
}

static void process_radio_command(racer_motors_t *motors, const char *buffer)
{
    int left_command;
    int right_command;

    if (sscanf(buffer, "%d %d", &left_command, &right_command) == 2)
    {
        printf("RX: %s\r\n", buffer);
        racer_motors_set(motors, left_command, right_command);
        pio_output_toggle(LED_STATUS_PIO);
    }
    else
    {
        printf("Invalid RX: %s. Expected: left right\r\n", buffer);
        fflush(stdout);
    }
}

int main(void)
{
    racer_motors_t motors;
    nrf24_t *nrf;
    button_t button;
    int error;

    pio_config_set(LED_STATUS_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(LED_ERROR_PIO, PIO_OUTPUT_HIGH);

    error = racer_motors_init(&motors);
    if (error)
        panic(LED_ERROR_PIO, error);

    usb_serial_stdio_init();

    nrf = radio_link_init();
    if (! nrf)
        panic(LED_ERROR_PIO, 3);

    button = button_init(&button_cfg);
    if (! button)
        panic(LED_ERROR_PIO, 4);

    button_poll_count_set(BUTTON_POLL_COUNT(BUTTON_POLL_RATE));
    pacer_init(BUTTON_POLL_RATE);

    print_startup();

    while (1)
    {
        char buffer[RADIO_PAYLOAD_SIZE + 1];

        pacer_wait();

        button_poll(button);
        if (button_pushed_p(button))
        {
            racer_motors_stop(&motors);
            radio_link_stop_send(nrf);
        }

        if (radio_link_read(nrf, buffer))
            process_radio_command(&motors, buffer);
    }

    return 0;
}
