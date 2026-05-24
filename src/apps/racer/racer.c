/*
   Main program for the racer board.

   This file ties the small racer modules together:
       - radio packets tell the car what motor PWM values to use,
       - bumper and failsafe logic can stop the motors,
       - sleep, FPV, low-voltage, heartbeat, and LED tape are updated
         once per main-loop tick.
*/
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "usb_serial.h"
#include "pio.h"
#include "pacer.h"
#include "button.h"
#include "panic.h"
#include "target.h"
#include "delay.h"
#include "racer_motors.h"
#include "radio_link.h"
#include "racer_sleep.h"
#include "racer_low_voltage.h"
#include "racer_heartbeat.h"
#include "racer_bumper.h"
#include "racer_ledtape.h"
#include "racer_power.h"
#include "racer_fpv.h"

/* The main loop runs 100 times per second, so one loop tick is 10 ms. */
#define BUTTON_POLL_RATE 100
#define MAIN_LOOP_PERIOD_MS (1000u / BUTTON_POLL_RATE)

/*
   This packet format and these constants must match hat.c.
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
   These are the only RF channels the racer will use.  The hat uses the same
   table.  The counter chooses the channel, so both boards hop together.
*/
static const uint8_t link_hop_table[] = {
    2, 26, 62, 14, 74,
    38, 6, 54, 18, 70,
    34, 10, 46, 78, 22,
    58, 30, 66, 42, 50
};

/*
   Compact radio packet from the hat.  It stays well under the nRF24L01
   32-byte payload limit and carries only the values the racer needs.
*/
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

/*
   Radio link state:
       last_valid_counter    last packet counter accepted from the hat
       link_synced           true once the racer knows where the hat is
       last_valid_packet_ms  time of the last accepted packet, for failsafe
       scan indexes          remember where resync scanning should continue
*/
static uint32_t last_valid_counter = UINT32_MAX;
static bool link_synced = false;
static uint32_t last_valid_packet_ms = 0;
static uint8_t link_initial_scan_index = 0;
static uint8_t link_resync_offset = 1;

static uint8_t link_channel_for_counter(uint32_t counter)
{
    uint8_t hop_count = sizeof(link_hop_table) / sizeof(link_hop_table[0]);
    uint8_t hop_index;

    /* The hop is based only on the counter, so both devices move together. */
    hop_index = (counter / LINK_PACKETS_PER_HOP) % hop_count;

    return link_hop_table[hop_index];
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

static bool link_counter_is_newer(uint32_t new_counter, uint32_t last_counter)
{
    /*
       Subtracting the counters lets wraparound work.  For example, after
       0xffffffff the next valid counter is 0, and 0 - 0xffffffff is 1.
    */
    return (int32_t)(new_counter - last_counter) > 0;
}

static void link_scan_reset(void)
{
    link_initial_scan_index = 0;
    link_resync_offset = 1;
}

static bool link_counter_is_in_resync_window(uint32_t counter)
{
    uint32_t counter_age = counter - last_valid_counter;

    return (counter_age > 0) && (counter_age <= LINK_RESYNC_LOOKAHEAD);
}

static bool link_packet_header_ok(const link_control_packet_t *packet)
{
    return packet->version == LINK_VERSION
        && packet->sender_id == LINK_HAT_ID
        && packet->receiver_id == LINK_RACER_ID
        && packet->msg_type == LINK_MSG_CONTROL;
}

static bool link_packet_valid(const link_control_packet_t *packet)
{
    /* Reject anything that does not look like a current hat control packet. */
    if (! link_packet_header_ok(packet))
        return false;

    /* Reject packets that were edited or accidentally corrupted. */
    if (! link_check_packet(packet))
        return false;

    /* Reject old packets so a replayed radio message cannot drive the car. */
    if (! link_counter_is_newer(packet->counter, last_valid_counter))
        return false;

    return true;
}

static bool link_read_valid_packet(nrf24_t *nrf, link_control_packet_t *packet)
{
    /*
       nrf24_read() returns 0 when no packet is waiting.  If a packet is
       waiting, validate it before the motor code ever sees it.
    */
    if (! nrf24_read(nrf, packet, sizeof(*packet)))
        return false;

    return link_packet_valid(packet);
}

static void link_listen_on_channel(nrf24_t *nrf, uint8_t channel,
                                   uint8_t *current_channel)
{
    /*
       Setting the channel over SPI takes time, so only do it when the channel
       really changed.  The current_channel variable is our local memory of the
       last channel we asked the radio to use.
    */
    if (*current_channel != channel)
    {
        nrf24_set_channel(nrf, channel);
        *current_channel = channel;
    }

    nrf24_listen(nrf);
}

static bool link_scan_all_channels(nrf24_t *nrf, link_control_packet_t *packet,
                                   uint8_t *current_channel)
{
    uint8_t hop_count = sizeof(link_hop_table) / sizeof(link_hop_table[0]);
    uint8_t channel = link_hop_table[link_initial_scan_index];

    /* When never synced, scan every known hop channel, one per poll. */
    link_initial_scan_index++;
    if (link_initial_scan_index >= hop_count)
        link_initial_scan_index = 0;

    link_listen_on_channel(nrf, channel, current_channel);
    delay_ms(LINK_RESYNC_DWELL_MS);

    return link_read_valid_packet(nrf, packet);
}

static bool link_scan_future_counters(nrf24_t *nrf,
                                      link_control_packet_t *packet,
                                      uint8_t *current_channel)
{
    uint32_t candidate_counter = last_valid_counter + link_resync_offset;

    /*
       After packets are missed, scan only the expected future counters.
       One dwell is attempted per poll so the failsafe can still run.
    */
    link_resync_offset++;
    if (link_resync_offset > LINK_RESYNC_LOOKAHEAD)
        link_resync_offset = 1;

    link_listen_on_channel(nrf,
                           link_channel_for_counter(candidate_counter),
                           current_channel);
    delay_ms(LINK_RESYNC_DWELL_MS);

    return link_read_valid_packet(nrf, packet)
        && link_counter_is_in_resync_window(packet->counter);
}

static bool link_poll(nrf24_t *nrf, link_control_packet_t *packet,
                      uint8_t *current_channel, uint32_t now_ms)
{
    /*
       Normal case: the racer is synced, so it listens on the exact channel
       where the hat should send the next counter.
    */
    if (link_synced)
    {
        /* When synced, listen where the next valid counter should be. */
        link_listen_on_channel(nrf,
                               link_channel_for_counter(last_valid_counter + 1),
                               current_channel);

        if (link_read_valid_packet(nrf, packet))
            return true;

        if ((uint32_t)(now_ms - last_valid_packet_ms) <= LINK_RESYNC_GRACE_MS)
            return false;
    }

    /*
       Recovery case: if we have never synced, try every hop-table channel.
       If we have synced before, scan only the future counters we expect.
    */
    if (last_valid_counter == UINT32_MAX)
        return link_scan_all_channels(nrf, packet, current_channel);

    return link_scan_future_counters(nrf, packet, current_channel);
}

static void print_startup(void)
{
    printf("\r\nWacky Racer radio PWM motor control ready.\r\n");
    printf("RX input format: checked control packet\r\n");
    printf("Duty range: -100 to 100\r\n");
    printf("BUMPER_PIO sends STOP and disables the H-bridge for 5 seconds\r\n");
    printf("SLEEP_PIO toggles MCU sleep on/off\r\n");
    printf("BUTTON_PIO toggles FPV on/off\r\n");
    printf("BUTTON_PIO2 cycles LED tape: standard, rainbow, blocks, off\r\n");
    printf("Radio hops every %u packets, about %u ms\r\n",
           LINK_PACKETS_PER_HOP,
           LINK_PACKETS_PER_HOP * LINK_TX_PERIOD_MS);
    printf("Failsafe stops motors after %u ms without a valid packet\r\n",
           LINK_FAILSAFE_MS);
    fflush(stdout);
}

static void process_radio_command(racer_motors_t *motors,
                                  const link_control_packet_t *packet)
{
    /* The packet has already passed all link checks before this function. */
    printf("RX counter %lu: %d %d\r\n",
           (unsigned long)packet->counter,
           packet->left_pwm,
           packet->right_pwm);
    racer_motors_set(motors, packet->left_pwm, packet->right_pwm);
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
    uint8_t current_radio_channel = 0xffu;
    uint32_t now_ms = 0;
    int error;

    /* Turn on board power rails and configure simple status systems first. */
    racer_power_init();
    racer_low_voltage_init();
    racer_heartbeat_init();

    /* Motors are safety-critical, so stop immediately if setup fails. */
    error = racer_motors_init(&motors);
    if (error)
        panic(LED_ERROR_PIO, error);

    usb_serial_stdio_init();

    /*
       Start listening on the channel for counter 0.  last_valid_counter is
       UINT32_MAX, so last_valid_counter + 1 wraps to 0.
    */
    radio_channel = link_channel_for_counter(last_valid_counter + 1);
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
    printf("Starting radio channel: %u\r\n", radio_channel);
    fflush(stdout);

    while (1)
    {
        link_control_packet_t packet;
        bool bumper_pushed;

        /* This keeps the whole program running at BUTTON_POLL_RATE. */
        pacer_wait();
        now_ms += MAIN_LOOP_PERIOD_MS;

        /* These modules are small tasks that run once per main-loop tick. */
        racer_heartbeat_update();
        racer_low_voltage_update();
        racer_fpv_update(&fpv);

        /*
           The bumper returns true only on the first press.  Its active state
           stays true for the full 5-second H-bridge disable window, and the
           LED tape uses that active state for the red flash pattern.
        */
        bumper_pushed = racer_bumper_update(&bumper);
        racer_ledtape_bumper_set(&ledtape, racer_bumper_is_active(&bumper));
        racer_ledtape_update(&ledtape);

        if (bumper_pushed)
        {
            racer_motors_stop(&motors);
            radio_link_stop_send(nrf);
        }

        racer_sleep_poll(&sleep);

        if (racer_sleep_toggle_requested_p(&sleep))
        {
            /* Stop anything that should not run while the board is asleep. */
            racer_motors_stop(&motors);
            racer_sleep_arm(&sleep);
            racer_ledtape_set(&ledtape, false);
            racer_power_sleep_enter();
            racer_sleep_wait_for_wake(&sleep);
            racer_power_sleep_exit();
            racer_fpv_apply(&fpv);
            racer_bumper_reset(&bumper);
            racer_ledtape_bumper_set(&ledtape, false);
            racer_ledtape_set(&ledtape, true);
            racer_sleep_finish(&sleep);

            /* The radio was power-cycled during sleep, so initialise it again. */
            radio_channel = link_channel_for_counter(last_valid_counter + 1);
            nrf = radio_link_init(radio_channel);
            current_radio_channel = 0xffu;
            link_synced = false;
            if (! nrf)
                panic(LED_ERROR_PIO, 8);

            printf("Starting radio channel: %u\r\n", radio_channel);
            fflush(stdout);
            continue;
        }

        if (link_poll(nrf, &packet, &current_radio_channel, now_ms))
        {
            process_radio_command(&motors, &packet);

            /*
               The racer only advances/resyncs after a valid packet, so old
               recorded packets are ignored instead of reaching the motors.
            */
            last_valid_counter = packet.counter;
            last_valid_packet_ms = now_ms;
            link_synced = true;
            link_scan_reset();

            link_listen_on_channel(nrf,
                                   link_channel_for_counter(last_valid_counter + 1),
                                   &current_radio_channel);
        }

        if ((uint32_t)(now_ms - last_valid_packet_ms) >= LINK_FAILSAFE_MS)
        {
            /* No good radio packet recently: stop instead of coasting blindly. */
            racer_motors_stop(&motors);
            link_synced = false;
        }
    }

    return 0;
}
