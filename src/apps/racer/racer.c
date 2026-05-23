#include <stdio.h>

#include "usb_serial.h"
#include "pio.h"
#include "pacer.h"
#include "button.h"
#include "panic.h"
#include "target.h"
#include "racer_motors.h"
#include "radio_link.h"
#include "racer_sleep.h"
#include "racer_radio_channel.h"
#include "racer_low_voltage.h"
#include "racer_heartbeat.h"
#include "racer_bumper.h"
#include "racer_ledtape.h"
#include "racer_power.h"
#include "racer_fpv.h"

#define BUTTON_POLL_RATE 100

static void print_startup(void)
{
    printf("\r\nWacky Racer radio PWM motor control ready.\r\n");
    printf("RX input format: left right\r\n");
    printf("Duty range: -100 to 100\r\n");
    printf("BUMPER_PIO sends STOP and disables the H-bridge for 5 seconds\r\n");
    printf("SLEEP_PIO toggles MCU sleep on/off\r\n");
    printf("BUTTON_PIO toggles FPV on/off\r\n");
    printf("BUTTON_PIO2 cycles LED tape: green, red, blue, rainbow, blocks, off\r\n");
    printf("DIP switches choose radio channel: base 84 plus DIP value\r\n");
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
    racer_sleep_t sleep;
    racer_bumper_t bumper;
    racer_ledtape_t ledtape;
    racer_fpv_t fpv;
    nrf24_t *nrf;
    uint8_t radio_channel;
    int error;

    racer_power_init();
    racer_radio_channel_init();
    racer_low_voltage_init();
    racer_heartbeat_init();

    error = racer_motors_init(&motors);
    if (error)
        panic(LED_ERROR_PIO, error);

    usb_serial_stdio_init();

    radio_channel = racer_radio_channel_get();
    nrf = radio_link_init(radio_channel);
    if (! nrf)
        panic(LED_ERROR_PIO, 3);

    error = racer_sleep_init(&sleep);
    if (error)
        panic(LED_ERROR_PIO, 5);

    error = racer_bumper_init(&bumper);
    if (error)
        panic(LED_ERROR_PIO, 6);

    error = racer_ledtape_init(&ledtape);
    if (error)
        panic(LED_ERROR_PIO, 7);

    error = racer_fpv_init(&fpv);
    if (error)
        panic(LED_ERROR_PIO, 9);

    button_poll_count_set(BUTTON_POLL_COUNT(BUTTON_POLL_RATE));
    pacer_init(BUTTON_POLL_RATE);

    print_startup();
    printf("Radio channel: %u\r\n", radio_channel);
    fflush(stdout);

    while (1)
    {
        char buffer[RADIO_PAYLOAD_SIZE + 1];

        pacer_wait();

        racer_heartbeat_update();
        racer_low_voltage_update();
        racer_ledtape_update(&ledtape);
        racer_fpv_update(&fpv);

        if (racer_bumper_update(&bumper))
        {
            racer_motors_stop(&motors);
            radio_link_stop_send(nrf);
        }

        racer_sleep_poll(&sleep);

        if (racer_sleep_toggle_requested_p(&sleep))
        {
            racer_motors_stop(&motors);
            racer_sleep_arm(&sleep);
            racer_ledtape_set(&ledtape, false);
            racer_power_sleep_enter();
            racer_sleep_wait_for_wake(&sleep);
            racer_power_sleep_exit();
            racer_fpv_apply(&fpv);
            racer_bumper_reset(&bumper);
            racer_ledtape_set(&ledtape, true);
            racer_sleep_finish(&sleep);

            radio_channel = racer_radio_channel_get();
            nrf = radio_link_init(radio_channel);
            if (! nrf)
                panic(LED_ERROR_PIO, 8);

            printf("Radio channel: %u\r\n", radio_channel);
            fflush(stdout);
            continue;
        }

        if (radio_link_read(nrf, buffer))
            process_radio_command(&motors, buffer);
    }

    return 0;
}
