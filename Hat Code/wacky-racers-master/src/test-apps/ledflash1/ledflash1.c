/* File:   ledflash1.c
   Author: M. P. Hayes, UCECE
   Date:   15 May 2007
   Descr:  Flash an LED using a paced loop
*/
#include <pio.h>
#include "target.h"
#include "pacer.h"

#include <stdint.h>

/* Define PACER rate in Hz.  The minimum is 6 Hz for a 96 MHz CPU clock.  */
#define PACER_RATE 200

/* Define LED flash rate in Hz.  */
#define LED_FLASH_RATE 2


int
main (void)
{
    uint32_t ticks = 0;

    /* Configure STATUS LED PIO as output and set high.  The LED should
       turn on if wired active-high.  */
    pio_config_set (LED_ERROR_PIO, PIO_OUTPUT_LOW);
    pio_config_set (LED_STATUS_PIO, PIO_OUTPUT_LOW);

    pacer_init (PACER_RATE);

    while (1)
    {
        /* Wait until next clock tick.  */
        pacer_wait ();

        ticks++;

        if (ticks >= PACER_RATE / LED_FLASH_RATE / 2)
        {
            ticks = 0;

            /* Toggle LED.  */
            pio_output_toggle (LED_ERROR_PIO);
            pio_output_toggle (LED_RED_PIO);
        }
    }
}
