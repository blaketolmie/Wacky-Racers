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
#include "mcu_sleep.h"


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
#define TRANSMIT_PWM_FREQUENCY 20
#define LED_STRIP_FREQUENCY 50
#define LOW_VOLTAGE_FREQUENCY 3

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

    // Variables
    nrf24_t *nrf;
    static int led_pos = 0;
    static int led_pos2 = NUM_LEDS / 2; //start the second led on the other side
    char buffer[RADIO_PAYLOAD_SIZE + 1];
    int channel = 84;
    


    // Initialisation
    pio_config_set(LED_STATUS_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(LED_ERROR_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(LED_GREEN_PIO, PIO_OUTPUT_HIGH);
    pio_config_set(BUTTON_1_PIO, PIO_PULLUP);
    pio_config_set(BUTTON_SLEEP_PIO, PIO_PULLUP);
    usb_serial_stdio_init();
    nrf = hat_radio_init();
    if (! nrf)
        panic(LED_ERROR_PIO, 2);
    radio_channel_init();
    imu_init();
    pacer_init(PACER_RATE);
    ledbuffer_t *leds = ledbuffer_init (LEDTAPE_PIO, NUM_LEDS);
    init_low_voltage();

    print_startup();

    while (1)
    {
        pacer_wait();
        
        // Check whether channel has been changed using dip switch
        if (channel_check_ticks++ >= CHANNEL_CHECK_TICKS && channel != radio_channel_get())
        {
            channel = radio_channel_get();
            nrf24_set_channel(nrf, channel);
            channel_check_ticks = 0;
        }

        // Blinky
        if (blinky_ticks++ >= BLINKY_TICKS)
        {
            pio_output_toggle(LED_STATUS_PIO);
            printf("SLEEP BTN: %d\r\n", pio_input_get(BUTTON_SLEEP_PIO));
            fflush(stdout);  
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

        // button 1 triggers bumper hit for testing
        if (!pio_input_get(BUTTON_1_PIO) && !bumper_hit_is_active())
        {
            bumper_hit_start();
        }

        // sleep button: shut everything down and wait for wakeup
        if (!pio_input_get(BUTTON_SLEEP_PIO))
        {   
            printf("sleep pressed\r\n");
            fflush(stdout);
            pio_config_set(LED_GREEN_PIO, PIO_OUTPUT_HIGH);

            // send zero PWM so the car stops
            hat_radio_pwm_send(nrf, 0, 0);

            // power down peripherals
            bumper_hit_stop();
            ledbuffer_clear(leds);
            ledbuffer_write(leds);
            pio_config_set(RADIO_OFF_PIO, PIO_OUTPUT_LOW);
            pio_config_set(IMU_OFF, PIO_OUTPUT_LOW);

            static const mcu_sleep_wakeup_t wakeups[] = {
                {.pio = BUTTON_SLEEP_PIO, .active_high = false}
            };
            static const mcu_sleep_cfg_t sleep_cfg = {
                .mode = MCU_SLEEP_MODE_WAIT,
                .debounce = 0,
                .num_wakeups = 1,
                .wakeups = wakeups
            };
            mcu_sleep(&sleep_cfg);

            // wake up: re-enable peripherals
            pio_config_set(RADIO_OFF_PIO, PIO_OUTPUT_HIGH);
            pio_config_set(IMU_OFF, PIO_OUTPUT_HIGH);
            pio_config_set(LED_GREEN_PIO, PIO_OUTPUT_LOW);
            nrf = hat_radio_init();
            imu_init();
        }

        // TX PWM values over radio, send zero if bumper hit active
        if (transmit_pwm_ticks++ >= TRANSMIT_PWM_TICKS)
        {
            int left_pwm, right_pwm;
            if (bumper_hit_is_active())
            {
                left_pwm = 0;
                right_pwm = 0;
            }
            else
            {
                get_pwm(&left_pwm, &right_pwm);
            }

            if (!hat_radio_pwm_send(nrf, left_pwm, right_pwm))
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
            transmit_pwm_ticks = 0;
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
