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
#include "radio_channel.h"
#include "ledbuffer.h"
#include "low_voltage.h"


static void print_startup(void)
{
    printf("\r\nWacky Hat radio PWM sender ready.\r\n");
    printf("Sending left/right PWM values once per second from 100 to -100\r\n");
    printf("LED_STATUS toggles after STOP is received\r\n");
    fflush(stdout);
}

// task frequencies (Hz)
#define CHANNEL_CHECK_FREQUENCY 1
#define BLINKY_FREQUENCY 2
#define TRANSMIT_PWM_FREQUENCY 5
#define LED_STRIP_FREQUENCY 50
#define LOW_VOLTAGE_FREQUENCY 1

// task ticks 
#define CHANNEL_CHECK_TICKS (PACER_RATE/CHANNEL_CHECK_FREQUENCY)
#define BLINKY_TICKS (PACER_RATE/BLINKY_FREQUENCY)
#define TRANSMIT_PWM_TICKS (PACER_RATE/TRANSMIT_PWM_FREQUENCY)
#define LED_STRIP_TICKS (PACER_RATE/LED_STRIP_FREQUENCY)
#define LOW_VOLTAGE_TICKS (PACER_RATE/LOW_VOLTAGE_FREQUENCY)

// Misc. Definitions
#define VOLTAGE_THRESHHOLD 5000


int main(void)
{
    // Schedular variables
    int channel_check_ticks = 0;
    int blinky_ticks = 0;
    int transmit_pwm_ticks = 0;
    int led_strip_ticks = 0;
    int low_voltage_ticks = 0;

    nrf24_t *nrf;

    


    // Initialisation
    pio_config_set(LED_STATUS_PIO, PIO_OUTPUT_LOW);
    pio_config_set(LED_ERROR_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(BUTTON_1_PIO, PIO_PULLUP);   // button for testing bumper hit
    usb_serial_stdio_init();
    nrf = hat_radio_init();
    if (! nrf)
        panic(LED_ERROR_PIO, 2);
    radio_channel_init();
    imu_init();
    pacer_init(PACER_RATE);
    ledbuffer_t *leds = ledbuffer_init (LEDTAPE_PIO, NUM_LEDS);
    init_low_voltage();

    static int led_pos = 0;
    static int led_pos2 = NUM_LEDS / 2; //start the second led on the other side

    print_startup();

    char buffer[RADIO_PAYLOAD_SIZE + 1];

    while (1)
    {
        pacer_wait();
        
        // Check whether channel has been changed using dip switch
        if (channel_check_ticks++ >= CHANNEL_CHECK_TICKS)
        {
            nrf24_set_channel(nrf, radio_channel_get());
            channel_check_ticks = 0;
        }

        // Blinky
        if (blinky_ticks++ >= BLINKY_TICKS)
        {
            pio_output_toggle(LED_STATUS_PIO);
            blinky_ticks = 0;
        }

        // Radio RX - happens every tick
        if (hat_radio_read(nrf, buffer))
        {
            printf("RX: %s\r\n", buffer);
            if (hat_radio_stop_received(buffer))
                bumper_hit_start();
            fflush(stdout);
        }

        // button press triggers bumper hit for testing
        if (!pio_input_get(BUTTON_1_PIO) && !bumper_hit_is_active())
            bumper_hit_start();

        // TX PWM values over radio
        if (transmit_pwm_ticks++ >= TRANSMIT_PWM_TICKS)
        {
            int left_pwm, right_pwm;
            get_pwm(&left_pwm, &right_pwm);
            // printf("Left = %d    Right = %d\n", left_pwm, right_pwm);
            transmit_pwm_ticks = 0;

            if (! hat_radio_pwm_send(nrf, left_pwm, right_pwm))
            {
                pio_output_set(LED_ERROR_PIO, LED_ACTIVE);
                printf("TX failed: %d %d\r\n", left_pwm, right_pwm);
            }
            else
            {
                pio_output_set(LED_ERROR_PIO, !LED_ACTIVE);
                printf("TX pwm: %d %d\r\n", left_pwm, right_pwm);
            }
            fflush(stdout);
        }

        // bumper hit takes over the LED strip while active
        if (bumper_hit_is_active())
        {
            bumper_hit_update(leds);
        }
        else if (led_strip_ticks++ >= LED_STRIP_TICKS)
        {
            ledbuffer_clear(leds);
            ledbuffer_set(leds, (led_pos - 1 + NUM_LEDS) % NUM_LEDS, 0, 0, 10);
            ledbuffer_set(leds, led_pos, 0, 0, 255);
            ledbuffer_set(leds, (led_pos + 1) % NUM_LEDS, 0, 0, 10);

            ledbuffer_set(leds, (led_pos2 - 1 + NUM_LEDS) % NUM_LEDS, 10, 0, 0);
            ledbuffer_set(leds, led_pos2, 255, 0, 0);
            ledbuffer_set(leds, (led_pos2 + 1) % NUM_LEDS, 10, 0, 0);

            ledbuffer_write(leds);

            led_pos = (led_pos + 1) % NUM_LEDS;
            led_pos2 = (led_pos2 - 1 + NUM_LEDS) % NUM_LEDS;
            led_strip_ticks = 0;
        }



        // Low Voltage
        if (low_voltage_ticks++ >= LOW_VOLTAGE_TICKS)
        {
            if (get_battery_voltage() <= VOLTAGE_THRESHHOLD)
            {
                low_power(true);
            }
            else
            {
                low_power(false);
            }
            low_voltage_ticks = 0;
        }

    }

    return 0;
}
