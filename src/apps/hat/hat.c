#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "usb_serial.h"
#include "pio.h"
#include "pacer.h"
#include "panic.h"
#include "target.h"
#include "hat_radio.h"
#include "control_logic.h"
#include "bumper_hit.h"
#include "ledbuffer.h"
#include "low_voltage.h"
#include "mcu_sleep.h"

/*
   This packet format and these constants must match racer.c.
   The counter rejects replayed packets, and a recorded packet may still
   have a valid nRF24L01 CRC, so the MCU also checks the packet contents.
   This does not stop deliberate wideband jamming, but it stops simple
   record-and-loop replay from being accepted as fresh control data.
*/
#define LINK_VERSION             1u
#define LINK_HAT_ID              0xA1u
#define LINK_RACER_ID            0xB2u
#define LINK_MSG_CONTROL         0x01u
#define LINK_TX_PERIOD_MS        20u
#define LINK_PACKETS_PER_HOP     10u
#define LINK_FAILSAFE_MS         500u
#define LINK_RESYNC_LOOKAHEAD    60u
#define LINK_RESYNC_DWELL_MS     10u
#define LINK_RESYNC_GRACE_MS     120u
#define LINK_SECRET              0x5A3C9E27u

/*
   Channels 1-10 are left unused.  These channels are spaced 4 MHz apart,
   then ordered so each hop moves well away from the previous RF channel.
*/
static const uint8_t link_hop_table[] = {
    11, 43, 75, 27, 59, 15,
    47, 79, 31, 63, 19, 51,
    23, 55, 35, 67, 39, 71
};

static uint8_t link_current_channel = 0xffu;

typedef struct __attribute__((packed))
{
    uint8_t version;
    uint8_t sender_id;
    uint8_t receiver_id;
    uint8_t msg_type;
    uint32_t counter;
    int8_t left_pwm;
    int8_t right_pwm;
    uint8_t buttons;
    uint8_t flags;
    uint16_t check;
} link_control_packet_t;

static uint8_t link_channel_for_counter(uint32_t counter)
{
    uint8_t hop_count = sizeof(link_hop_table) / sizeof(link_hop_table[0]);
    uint8_t hop_index;

    /* The hop is based only on the counter, so both devices move together. */
    hop_index = (counter / LINK_PACKETS_PER_HOP) % hop_count;

    return link_hop_table[hop_index];
}

static void link_set_channel(nrf24_t *nrf, uint8_t channel)
{
    if (link_current_channel != channel)
    {
        nrf24_set_channel(nrf, channel);
        link_current_channel = channel;
        printf("Radio channel: %u\r\n", channel);
        fflush(stdout);
    }
}

static uint16_t link_make_check(const link_control_packet_t *packet)
{
    const uint8_t *bytes = (const uint8_t *)packet;
    uint32_t hash = LINK_SECRET;
    uint8_t i;

    /*
       The nRF24L01 CRC catches radio corruption.  This small keyed packet
       check is the MCU-level validity check for edited or replayed packets.
       It is intentionally simple, not a full cryptographic MAC.
    */
    for (i = 0; i < sizeof(*packet) - sizeof(packet->check); i++)
    {
        hash ^= bytes[i];
        hash *= 16777619u;
    }

    return (uint16_t)((hash & 0xffffu) ^ (hash >> 16));
}

static bool link_check_packet(const link_control_packet_t *packet)
{
    return packet->check == link_make_check(packet);
}

static int8_t link_pwm_to_int8(int pwm)
{
    if (pwm > 100)
        return 100;

    if (pwm < -100)
        return -100;

    return (int8_t)pwm;
}

static uint8_t link_control_send(nrf24_t *nrf, int left_pwm,
                                 int right_pwm, uint32_t counter)
{
    link_control_packet_t packet = {0};

    packet.version = LINK_VERSION;
    packet.sender_id = LINK_HAT_ID;
    packet.receiver_id = LINK_RACER_ID;
    packet.msg_type = LINK_MSG_CONTROL;
    packet.counter = counter;
    packet.left_pwm = link_pwm_to_int8(left_pwm);
    packet.right_pwm = link_pwm_to_int8(right_pwm);
    packet.buttons = 0;
    packet.flags = 0;
    packet.check = link_make_check(&packet);

    if (! link_check_packet(&packet))
        return 0;

    link_set_channel(nrf, link_channel_for_counter(counter));

    return nrf24_write(nrf, &packet, sizeof(packet));
}


static void print_startup(void)
{
    printf("\r\nWacky Hat radio PWM sender ready.\r\n");
    printf("Sending checked control packets every %u ms\r\n", LINK_TX_PERIOD_MS);
    printf("Radio hops every %u packets, about %u ms\r\n",
           LINK_PACKETS_PER_HOP,
           LINK_PACKETS_PER_HOP * LINK_TX_PERIOD_MS);
    printf("LED_STATUS toggles after STOP is received\r\n");
    fflush(stdout);
}

// task frequencies (Hz)
#define BLINKY_FREQUENCY 2
#define LED_STRIP_FREQUENCY 50
#define LOW_VOLTAGE_FREQUENCY 3

// task ticks 
#define BLINKY_TICKS (PACER_RATE/BLINKY_FREQUENCY)
#define TRANSMIT_PWM_TICKS ((int)(PACER_RATE * LINK_TX_PERIOD_MS / 1000))
#define LED_STRIP_TICKS (PACER_RATE/LED_STRIP_FREQUENCY)
#define LOW_VOLTAGE_TICKS (PACER_RATE/LOW_VOLTAGE_FREQUENCY)

// Misc. Definitions
#define VOLTAGE_THRESHHOLD 5000


int main(void)
{
    // Schedular variables
    int blinky_ticks = 0;
    int transmit_pwm_ticks = 0;
    int led_strip_ticks = 0;
    int low_voltage_ticks = 0;

    // Variables
    nrf24_t *nrf;
    static int led_pos = 0;
    static int led_pos2 = NUM_LEDS / 2; //start the second led on the other side
    static uint32_t tx_counter = 0;
    char buffer[RADIO_PAYLOAD_SIZE + 1];
    


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
    link_current_channel = 0xffu;
    link_set_channel(nrf, link_channel_for_counter(tx_counter));
    imu_init();
    pacer_init(PACER_RATE);
    ledbuffer_t *leds = ledbuffer_init (LEDTAPE_PIO, NUM_LEDS);
    init_low_voltage();

    print_startup();

    while (1)
    {
        pacer_wait();

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
            link_control_send(nrf, 0, 0, tx_counter);
            tx_counter++;

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
            link_current_channel = 0xffu;
            link_set_channel(nrf, link_channel_for_counter(tx_counter));
            imu_init();
        }

        // TX PWM values over radio, send zero if bumper hit active
        transmit_pwm_ticks++;
        if (transmit_pwm_ticks >= TRANSMIT_PWM_TICKS)
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

            if (!link_control_send(nrf, left_pwm, right_pwm, tx_counter))
            {
                pio_output_set(LED_ERROR_PIO, LED_ACTIVE);
                printf("TX failed: %d %d\r\n", left_pwm, right_pwm);
            }
            else
            {
                pio_output_set(LED_ERROR_PIO, !LED_ACTIVE);
                printf("TX pwm: %d %d\r\n", left_pwm, right_pwm);
            }
            /* Increment every scheduled transmit so the hat can hop away from
               a bad channel even when the send or ACK fails. */
            tx_counter++;
            link_set_channel(nrf, link_channel_for_counter(tx_counter));
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
