#include <stdio.h>
#include <stdbool.h>

#include "usb_serial.h"
#include "pio.h"
#include "pacer.h"
#include "panic.h"
#include "target.h"
#include "hat_radio.h"
#include "control_logic.h"
#include "bumper_hit.h"

#define TX_TICKS PACER_RATE

static void print_startup(void)
{
    printf("\r\nWacky Hat radio PWM sender ready.\r\n");
    printf("Sending left/right PWM values once per second from 100 to -100\r\n");
    printf("LED_STATUS toggles after STOP is received\r\n");
    fflush(stdout);
}

int main(void)
{
    nrf24_t *nrf;
    bool stop_received = false;
    uint8_t ticks = 0;
    int ticks_remaining = 0;


    pio_config_set(LED_STATUS_PIO, LED_ACTIVE);
    pio_config_set(LED_ERROR_PIO, !LED_ACTIVE);

    usb_serial_stdio_init();

    nrf = hat_radio_init();
    if (! nrf)
        panic(LED_ERROR_PIO, 2);

    imu_init();
    pacer_init(PACER_RATE);

    print_startup();

    while (1)
    {
        char buffer[RADIO_PAYLOAD_SIZE + 1];

        pacer_wait();

        if (hat_radio_read(nrf, buffer))
        {
            printf("RX: %s\r\n", buffer);
            if (hat_radio_stop_received(buffer))
            {
                stop_received = true;
                printf("STOP received\r\n");
            }
            fflush(stdout);
        }

        ticks++;
        if (ticks >= TX_TICKS)
        {
            int left_pwm, right_pwm;
            get_pwm(&left_pwm, &right_pwm);
            // printf("Left = %d    Right = %d\n", left_pwm, right_pwm);
            ticks = 0;

            if (! hat_radio_pwm_send(nrf, left_pwm, right_pwm))
            {
                pio_output_set(LED_ERROR_PIO, 1);
                printf("TX failed: %d %d\r\n", left_pwm, right_pwm);
            }
            else
            {
                pio_output_set(LED_ERROR_PIO, 0);
                printf("TX pwm: %d %d\r\n", left_pwm, right_pwm);
            }

            if (stop_received)
            {
                if (ticks_remaining == 0){
                    bumper_hit_start();
                }
                ticks_remaining = bumper_hit_update();
                printf("ticks remaining = %d\r\n", ticks_remaining);
                if (ticks_remaining == 0)
                {
                    stop_received = 0;
                }
            }
            fflush(stdout);
        }
    }

    return 0;
}
