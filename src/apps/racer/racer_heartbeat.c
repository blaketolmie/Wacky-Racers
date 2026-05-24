/*
   Heartbeat LED.

   This is a simple "the program is alive" indicator.  The main loop calls
   racer_heartbeat_update() at 100 Hz, and this module toggles the status LED
   every 50 ticks, which gives a steady blink.
*/
#include "racer_heartbeat.h"
#include "pio.h"
#include "target.h"

#define HEARTBEAT_LED_PIO LED_STATUS_PIO
#define HEARTBEAT_TICKS 50

void racer_heartbeat_init(void)
{
    /* LED_ACTIVE is low on this board, so OUTPUT_HIGH means off. */
    pio_config_set(HEARTBEAT_LED_PIO, PIO_OUTPUT_HIGH);
}

void racer_heartbeat_update(void)
{
    static uint8_t ticks = 0;

    /* Count main-loop ticks until it is time to flip the LED. */
    ticks++;
    if (ticks >= HEARTBEAT_TICKS)
    {
        ticks = 0;
        pio_output_toggle(HEARTBEAT_LED_PIO);
    }
}
